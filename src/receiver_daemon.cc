#include "nova/rpc/amqp.h"
#include "nova/db/api.h"
#include "nova/guest/apt.h"
#include "nova/ConfigFile.h"
#include "nova/flags.h"
#include <boost/format.hpp>
#include "nova/guest/guest.h"
#include "nova/guest/diagnostics.h"
#include "nova/guest/GuestException.h"
#include <boost/lexical_cast.hpp>
#include <memory>
#include "nova/db/mysql.h"
#include "nova/guest/mysql/MySqlMessageHandler.h"
#include <boost/optional.hpp>
#include "nova/rpc/receiver.h"
#include <json/json.h>
#include "nova/Log.h"
#include <sstream>
#include <boost/thread.hpp>
#include "nova/guest/utils.h"


/* In release mode, all errors should be caught so the guest will not die.
 * If you want to see the bugs in the debug mode, you can fiddle with it here.
 */
// #ifndef _DEBUG
#define START_THREAD_TASK() try {
#define END_THREAD_TASK(name)  \
     } catch(const std::exception & e) { \
        NOVA_LOG_ERROR2("Error in " name "! : %s", e.what()); \
     }
#define CATCH_RPC_METHOD_ERRORS
// #else
// #define START_THREAD_TASK() /**/
// #define END_THREAD_TASK(name) /**/
// #define CATCH_RPC_METHOD_ERRORS
// #endif

using nova::db::ApiPtr;
using nova::guest::apt::AptGuest;
using nova::guest::apt::AptMessageHandler;
using std::auto_ptr;
using boost::format;
using boost::optional;
using namespace nova;
using namespace nova::flags;
using namespace nova::guest;
using namespace nova::guest::diagnostics;
using namespace nova::db::mysql;
using namespace nova::guest::mysql;
using nova::db::NewService;
using namespace nova::rpc;
using nova::db::ServicePtr;
using std::string;


const char PERIODIC_MESSAGE [] =
    "{"
    "    'method':'periodic_tasks',"
    "    'args':{}"
    "}";

static bool quit;

/* This runs in a different thread and just sends messages to the queue,
 * which cause the periodic_tasks to run. This keeps us from having to ensure
 * everything else is thread-safe. */
//TODO(tim.simpson): Rename this to nova::service::Service.
class PeriodicTasker {

private:
    JsonObject message;
    MySqlConnectionWithDefaultDbPtr nova_db;
    NewService service_key;
    MySqlAppStatusPtr status_updater;

public:
    PeriodicTasker(MySqlConnectionWithDefaultDbPtr nova_db,
                   MySqlAppStatusPtr status_updater, NewService service_key)
      : message(PERIODIC_MESSAGE),
        nova_db(nova_db),
        service_key(service_key),
        status_updater(status_updater)
    {
    }

    void ensure_db() {
        nova_db->ensure();
    }

    void loop(unsigned long periodic_interval, unsigned long report_interval) {
        unsigned long next_periodic_task = periodic_interval;
        unsigned long next_reporting = report_interval;
        while(!quit) {
            unsigned long wait_time = next_periodic_task < next_reporting ?
                next_periodic_task : next_reporting;
            NOVA_LOG_DEBUG2("Waiting for %lu seconds...", wait_time);
            boost::posix_time::seconds time(wait_time);
            boost::this_thread::sleep(time);
            next_periodic_task -= wait_time;
            next_reporting -= wait_time;

            if (next_periodic_task <= 0) {
                next_periodic_task += periodic_interval;
                periodic_tasks();
            }
            if (next_reporting <= 0) {
                next_reporting += report_interval;
                report_state();
            }
        }
    }

    void periodic_tasks() {
        START_THREAD_TASK();
            NOVA_LOG_DEBUG2("Running periodic tasks...");
            status_updater->update();
            Log::rotate_logs_if_needed();
        END_THREAD_TASK("periodic_tasks()");
    }

    void report_state() {
        START_THREAD_TASK();
            ensure_db();
            ApiPtr api = nova::db::create_api(nova_db);
            ServicePtr service = api->service_create(service_key);
            service->report_count ++;
            api->service_update(*service);
        END_THREAD_TASK("report_state()");
    }
};


AmqpConnectionPtr make_amqp_connection(FlagValues & flags) {
    return AmqpConnection::create(flags.rabbit_host(), flags.rabbit_port(),
        flags.rabbit_userid(), flags.rabbit_password(),
        flags.rabbit_client_memory());

}

int main(int argc, char* argv[]) {
    quit = false;



#ifndef _DEBUG

    try {

#endif
        // Initialize MySQL libraries. This should be done before spawning
        // threads.
        MySqlApiScope mysql_api_scope;

        /* Grab flag values. */
        FlagValues flags(FlagMap::create_from_args(argc, argv, true));

        /* Initialize logging. */
        optional<LogFileOptions> log_file_options;
        if (flags.log_file_path()) {
            LogFileOptions ops(flags.log_file_path().get(),
                               flags.log_file_max_size(),
                               flags.log_file_max_time(),
                               flags.log_file_max_old_files().get_value_or(30));
            log_file_options = optional<LogFileOptions>(ops);
        } else {
            log_file_options = boost::none;
        }
        LogOptions log_options(log_file_options,
                               flags.log_use_std_streams());

        LogApiScope log_api_scope(log_options);

        /* Create connection to Nova database. */
        MySqlConnectionWithDefaultDbPtr nova_db(
            new MySqlConnectionWithDefaultDb(
                flags.nova_sql_host(), flags.nova_sql_user(),
                flags.nova_sql_password(),
                flags.nova_sql_database()));

        /* Create JSON message handlers. */
        const int handler_count = 4;
        MessageHandlerPtr handlers[handler_count];

        /* Create Apt Guest */
        AptGuest apt_worker(flags.apt_use_sudo());
        handlers[0].reset(new AptMessageHandler(&apt_worker));

        /* Create MySQL updater. */
        MySqlAppStatusPtr mysql_status_updater(new MySqlAppStatus(
            nova_db,
            flags.guest_ethernet_device(),
            flags.nova_db_reconnect_wait_time(),
            flags.preset_instance_id()));

        /* Create MySQL Guest. */
        handlers[1].reset(new MySqlMessageHandler());

        handlers[2].reset(new MySqlAppMessageHandler(
            apt_worker, mysql_status_updater,
            flags.mysql_state_change_wait_time()));

        /* Create the Interrogator for the guest. */
        Interrogator interrogator;
        handlers[3].reset(new InterrogatorMessageHandler(interrogator));

        /* Set host value. */
        string actual_host = nova::guest::utils::get_host_name();
        string host = flags.host().get_value_or(actual_host.c_str());

        /* Create tasker. */
        NewService service_key;
        service_key.availability_zone = flags.node_availability_zone();
        service_key.binary = "nova-guest";
        service_key.host = host;
        service_key.topic = "guest";  // Real nova takes binary after "nova-".
        PeriodicTasker tasker(nova_db,
                              mysql_status_updater, service_key);

        /* Create AMQP connection. */
        string topic = "guest.";
        topic += nova::guest::utils::get_host_name();

        quit = false;

        // This starts the thread.
        boost::thread workerThread(&PeriodicTasker::loop, &tasker,
                                       flags.periodic_interval(),
                                       flags.report_interval());
        // Before we go any further, lets all just chill out and take a nap.
        // TODO(tim.simpson) For reasons I can only assume relate to an even
        // worse bug I have been able to uncover, sleeping here keeps Sneaky
        // Pete from morphing into Fat Pete (inexplicable 60 MB virtual memory
        // increase).
        boost::this_thread::sleep(boost::posix_time::seconds(3));

        NOVA_LOG_INFO("Creating listener...");

        ResilentReceiver receiver(flags.rabbit_host(), flags.rabbit_port(), //<-- here?!
                    flags.rabbit_userid(), flags.rabbit_password(),
                    flags.rabbit_client_memory(), topic.c_str(),
                    flags.control_exchange(),  flags.rabbit_reconnect_wait_time());

        while(!quit) {
            GuestInput input = receiver.next_message();
            NOVA_LOG_INFO2("method=%s", input.method_name.c_str());

            GuestOutput output;

            #ifdef CATCH_RPC_METHOD_ERRORS
            try {
            #endif
                for (int i = 0; i < handler_count && !output.result; i ++) {
                    output.result = handlers[i]->handle_message(input);
                }
                if (!output.result) {
                    NOVA_LOG_ERROR("No method found!")
                    throw GuestException(GuestException::NO_SUCH_METHOD);
                }
                output.failure = boost::none;
            #ifdef CATCH_RPC_METHOD_ERRORS
            } catch(const std::exception & e) {
                NOVA_LOG_ERROR2("Error running method %s : %s",
                           input.method_name.c_str(), e.what());
                NOVA_LOG_ERROR(e.what());
                output.result.reset();
                output.failure = e.what();
            }
            #endif

            receiver.finish_message(output);
        }

#ifndef _DEBUG
    } catch (const std::exception & e) {
        NOVA_LOG_ERROR2("Error: %s", e.what());
    }
#endif
    return 0;
}
