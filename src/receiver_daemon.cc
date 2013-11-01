#include "nova/rpc/amqp.h"
#include "nova/guest/apt.h"
#include "nova/ConfigFile.h"
#include "nova/flags.h"
#include <boost/format.hpp>
#include "nova/guest/guest.h"
#include "nova/guest/diagnostics.h"
#include <boost/foreach.hpp>
#include "nova/guest/GuestException.h"
#include "nova/guest/HeartBeat.h"
#include <boost/lexical_cast.hpp>
#include <iostream>
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
#include <vector>


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
using namespace nova::rpc;
using std::string;
using std::vector;

// Begin anonymous namespace.
namespace {

const char PERIODIC_MESSAGE [] =
    "{"
    "    'method':'periodic_tasks',"
    "    'args':{}"
    "}";

/* Handles receiving and processing messages. */
void message_loop(ResilentReceiver & receiver,
                  vector<MessageHandlerPtr> & handlers);

/* This is kind of a joke. There is no polite way to quit Sneaky. :p */
static const bool quit = false;

/**
 * Sends updates back to Reddwarf regarding the status of both
 * Sneaky Pete and the MySQL application.
 */
class PeriodicTasker {

private:
    HeartBeat & heart_beat;
    JsonObject message;
    MySqlConnectionWithDefaultDbPtr nova_db;
    MySqlAppStatusPtr status_updater;

public:
    PeriodicTasker(HeartBeat & heart_beat, MySqlAppStatusPtr status_updater)
      : heart_beat(heart_beat),
        message(PERIODIC_MESSAGE),
        status_updater(status_updater)
    {
    }

    void loop(unsigned long periodic_interval, unsigned long report_interval) {
        unsigned long next_periodic_task = periodic_interval;
        unsigned long next_reporting = report_interval;
        while(!quit) {
            unsigned long wait_time = next_periodic_task < next_reporting ?
                next_periodic_task : next_reporting;
            NOVA_LOG_TRACE2("Waiting for %lu seconds...", wait_time);
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
            NOVA_LOG_TRACE2("Running periodic tasks...");
            status_updater->update();
            Log::rotate_logs_if_needed();
        END_THREAD_TASK("periodic_tasks()");
    }

    void report_state() {
        START_THREAD_TASK();
            this->heart_beat.update();
        END_THREAD_TASK("report_state()");
    }
};


AmqpConnectionPtr make_amqp_connection(FlagValues & flags) {
    return AmqpConnection::create(flags.rabbit_host(), flags.rabbit_port(),
        flags.rabbit_userid(), flags.rabbit_password(),
        flags.rabbit_client_memory());

}

void initialize_and_run(FlagValues & flags) {
    NOVA_LOG_INFO(" ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^");
    NOVA_LOG_INFO(" ^ ^ ^                                       ^");
    NOVA_LOG_INFO(" ^ '  '       -----REDDWARF-GUEST-AGENT----- ^");
    NOVA_LOG_INFO(" ^ \\`-'/        -------Sneaky--Pete-------   ^");
    NOVA_LOG_INFO(" ^   |__                                     ^");
    NOVA_LOG_INFO(" ^  /                         starting now...^");
    NOVA_LOG_INFO(" ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^");

    // Initialize MySQL libraries. This should be done before spawning
    // threads.
    MySqlApiScope mysql_api_scope;

    /* Create connection to Nova database. */
    MySqlConnectionWithDefaultDbPtr nova_db(
        new MySqlConnectionWithDefaultDb(
            flags.nova_sql_host(), flags.nova_sql_user(),
            flags.nova_sql_password(),
            flags.nova_sql_database()));

    /* Create JSON message handlers. */
    vector<MessageHandlerPtr> handlers;

    /* Create Apt Guest */
    AptGuest apt_worker(flags.apt_use_sudo(),
                        flags.apt_self_package_name(),
                        flags.apt_self_update_time_out());
    MessageHandlerPtr handler_apt(new AptMessageHandler(&apt_worker));
    handlers.push_back(handler_apt);

    /* Create MySQL updater. */
    MySqlAppStatusPtr mysql_status_updater(new MySqlAppStatus(
        nova_db,
        flags.nova_db_reconnect_wait_time(),
        flags.guest_id()));

    /* Create MySQL Guest. */
    MessageHandlerPtr handler_mysql(new MySqlMessageHandler());
    handlers.push_back(handler_mysql);

    MessageHandlerPtr handler_mysql_app(new MySqlAppMessageHandler(
        apt_worker, mysql_status_updater,
        flags.mysql_state_change_wait_time()));
    handlers.push_back(handler_mysql_app);

    /* Create the Interrogator for the guest. */
    Interrogator interrogator;
    MessageHandlerPtr handler_interrogator(
        new InterrogatorMessageHandler(interrogator));
    handlers.push_back(handler_interrogator);

    /* Set host value. */
    string actual_host = nova::guest::utils::get_host_name();
    string host = flags.host().get_value_or(actual_host.c_str());

    /* Create HeartBeat. */
    HeartBeat heart_beat(nova_db, flags.guest_id());

    /* Create tasker. */
    PeriodicTasker tasker(heart_beat, mysql_status_updater);

    /* Create AMQP connection. */
    string topic = str(format("guestagent.%s") % flags.guest_id());

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

    ResilentReceiver receiver(flags.rabbit_host(), flags.rabbit_port(),
                flags.rabbit_userid(), flags.rabbit_password(),
                flags.rabbit_client_memory(), topic.c_str(),
                flags.control_exchange(),  flags.rabbit_reconnect_wait_time());

    message_loop(receiver, handlers);
}

void message_loop(ResilentReceiver & receiver,
                  vector<MessageHandlerPtr> & handlers) {
    while(!quit) {
        GuestInput input = receiver.next_message();
        NOVA_LOG_INFO2("method=%s", input.method_name.c_str());

        GuestOutput output;

        #ifdef CATCH_RPC_METHOD_ERRORS
        try {
        #endif
            BOOST_FOREACH(MessageHandlerPtr & handler, handlers) {
                output.result = handler->handle_message(input);
                if (output.result) {
                    break;
                }
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
}

} // End of anonymous namespace.

/**
 * Initializes the flag config and log options before passing
 * the former to main_loop with all errors printed to STDERR in release mode.
 */
int main(int argc, char* argv[]) {
#ifndef _DEBUG
    try {
#endif
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
                               flags.log_use_std_streams(),
                               flags.log_show_trace());

        LogApiScope log_api_scope(log_options);

        #ifndef _DEBUG
            try {
        #endif
            initialize_and_run(flags);
        #ifndef _DEBUG
            } catch (const std::exception & e) {
                NOVA_LOG_ERROR2("Error: %s", e.what());
            }
        #endif

#ifndef _DEBUG
    } catch (const std::exception & e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
#endif
    return 0;
}
