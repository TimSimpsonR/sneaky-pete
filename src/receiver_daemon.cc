#include "nova/rpc/amqp.h"
#include "nova/db/api.h"
#include "nova/guest/apt.h"
#include "nova/ConfigFile.h"
#include "nova/flags.h"
#include <boost/format.hpp>
#include "nova/guest/guest.h"
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
/////////#ifndef _DEBUG
#define START_THREAD_TASK() try {
#define END_THREAD_TASK(name)  \
     } catch(const std::exception & e) { \
        log.error2("Error in " name "! : %s", e.what()); \
     }
#define CATCH_RPC_METHOD_ERRORS
////////#endif

using nova::db::ApiPtr;
using nova::guest::apt::AptGuest;
using nova::guest::apt::AptMessageHandler;
using std::auto_ptr;
using boost::format;
using boost::optional;
using namespace nova;
using namespace nova::flags;
using namespace nova::guest;
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
    string guest_ethernet_device;
    Log log;
    JsonObject message;
    MySqlConnectionPtr nova_db;
    string nova_db_name;
    bool periodically_update_sql_status;
    NewService service_key;

public:
    PeriodicTasker(MySqlConnectionPtr nova_db, string nova_db_name,
                   string guest_ethernet_device, NewService service_key)
      : guest_ethernet_device(guest_ethernet_device),
        log(),
        message(PERIODIC_MESSAGE),
        nova_db(nova_db),
        nova_db_name(nova_db_name),
        periodically_update_sql_status(true),
        service_key(service_key)
    {
    }

    void ensure_db() {
        nova_db->ensure();
        nova_db->use_database(nova_db_name.c_str());
    }

    void loop(unsigned long periodic_interval, unsigned long report_interval) {
        unsigned long next_periodic_task = periodic_interval;
        unsigned long next_reporting = report_interval;
        while(!quit) {
            unsigned long wait_time = next_periodic_task < next_reporting ?
                next_periodic_task : next_reporting;
            log.info2("Waiting for %lu seconds...", wait_time);
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
        log.info("Running periodic tasks...");
        if (periodically_update_sql_status) {
            START_THREAD_TASK();
                ensure_db();
                MySqlNovaUpdaterPtr updater = sql_updater();
                MySqlNovaUpdater::Status status = updater->get_local_db_status();
                updater->update_status(status);
            END_THREAD_TASK("periodic_tasks()");
        } else {
            log.info("...actually, not doing that because prepare is running.");
        }
    }

    void report_state() {
        START_THREAD_TASK();
            ensure_db();
            ApiPtr api = nova::db::create_api(nova_db, nova_db_name);
            ServicePtr service = api->service_create(service_key);
            service->report_count ++;
            api->service_update(*service);
        END_THREAD_TASK("report_state()");
    }

    void set_periodically_update_sql_status(bool value) {
        this->periodically_update_sql_status = value;
    }

    MySqlNovaUpdaterPtr sql_updater() const {
        MySqlNovaUpdaterPtr ptr(new MySqlNovaUpdater(
            nova_db, guest_ethernet_device.c_str()));
        return ptr;
    }
};


AmqpConnectionPtr make_amqp_connection(FlagValues & flags) {
    return AmqpConnection::create(flags.rabbit_host(), flags.rabbit_port(),
        flags.rabbit_userid(), flags.rabbit_password(),
        flags.rabbit_client_memory());

}

int main(int argc, char* argv[]) {
    quit = false;
    Log log;

#ifndef _DEBUG

    try {
        daemon(1,0);
#endif

        /* Grab flag values. */
        FlagValues flags(FlagMap::create_from_args(argc, argv, true));

        /* Create connection to Nova database. */
        MySqlConnectionPtr nova_db(new MySqlConnection(
            flags.nova_sql_host(), flags.nova_sql_user(),
            flags.nova_sql_password()));

        /* Create JSON message handlers. */
        const int handler_count = 2;
        MessageHandlerPtr handlers[handler_count];

        /* Create Apt Guest */
        AptGuest apt_worker(flags.apt_use_sudo());
        handlers[0].reset(new AptMessageHandler(&apt_worker));

        /* Create MySQL Guest. */
        MySqlMessageHandlerConfig mysql_config;
        mysql_config.apt = &apt_worker;
        mysql_config.guest_ethernet_device = flags.guest_ethernet_device();
        mysql_config.nova_db_host = flags.nova_sql_host();
        mysql_config.nova_db_name = flags.nova_sql_database();
        mysql_config.nova_db_password = flags.nova_sql_password();
        mysql_config.nova_db_user = flags.nova_sql_user();
        handlers[1].reset(new MySqlMessageHandler(mysql_config));

        /* Set host value. */
        string actual_host = nova::guest::utils::get_host_name();
        string host = flags.host().get_value_or(actual_host.c_str());

        /* Create tasker. */
        NewService service_key;
        service_key.availability_zone = flags.node_availability_zone();
        service_key.binary = "nova-guest";
        service_key.host = host;
        service_key.topic = "guest";  // Real nova takes binary after "nova-".
        PeriodicTasker tasker(nova_db, flags.nova_sql_database(),
                              flags.guest_ethernet_device(), service_key);


        /* Create AMQP connection. */
        string topic = "guest.";
        topic += nova::guest::utils::get_host_name();


        quit = false;

        /* Start periodic task / report thread. */
        boost::thread workerThread(&PeriodicTasker::loop, &tasker,
                                   flags.periodic_interval(),
                                   flags.report_interval());

        /* Create receiver. */
        ResilentReceiver receiver(flags.rabbit_host(), flags.rabbit_port(),
            flags.rabbit_userid(), flags.rabbit_password(),
            flags.rabbit_client_memory(), topic.c_str(),
            flags.control_exchange(),  flags.rabbit_reconnect_wait_time());

        while(!quit) {
            GuestInput input = receiver.next_message();
            log.info2("method=%s", input.method_name.c_str());

            GuestOutput output;

            if (input.method_name == "prepare") {
                tasker.set_periodically_update_sql_status(false);
            }
            #ifdef CATCH_RPC_METHOD_ERRORS
            try {
            #endif
                for (int i = 0; i < handler_count && !output.result; i ++) {
                    output.result = handlers[i]->handle_message(input);
                }
                if (!output.result) {
                    throw GuestException(GuestException::NO_SUCH_METHOD);
                }
                output.failure = boost::none;
            #ifdef CATCH_RPC_METHOD_ERRORS
            } catch(const std::exception & e) {
                log.error2("Error running method %s : %s",
                           input.method_name.c_str(), e.what());
                log.error(e.what());
                output.result.reset();
                output.failure = e.what();
            }
            #endif
            if (input.method_name == "prepare") {
                tasker.set_periodically_update_sql_status(true);
            }
            receiver.finish_message(output);
        }
#ifndef _DEBUG
    } catch (const std::exception & e) {
        log.error2("Error: %s", e.what());
    }
#endif
    return 0;
}
