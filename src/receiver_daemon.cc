#include "pch.hpp"
#include "nova/rpc/amqp.h"
#include "nova/guest/apt.h"
#include "nova/ConfigFile.h"
#include "nova/utils/Curl.h"
#include "nova/flags.h"
#include <boost/assign/list_of.hpp>
#include "nova/LogFlags.h"
#include <boost/format.hpp>
#include "nova/guest/guest.h"
#include "nova/guest/diagnostics.h"
#include "nova/guest/backup/BackupManager.h"
#include "nova/guest/backup/BackupMessageHandler.h"
#include "nova/guest/monitoring/monitoring.h"
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
#include "nova/utils/threads.h"
#include "nova/guest/utils.h"
#include <vector>


/* In release mode, all errors should be caught so the guest will not die.
 * If you want to see the bugs in the debug mode, you can fiddle with it here.
 */
// #ifndef _DEBUG
#define START_THREAD_TASK() try {
#define END_THREAD_TASK(name)  \
     } catch(const std::exception & e) { \
        NOVA_LOG_ERROR("Error in " name "! : %s", e.what()); \
     } catch(...) { \
        NOVA_LOG_ERROR("Error in " name "! Exception type unknown."); \
     }
#define CATCH_RPC_METHOD_ERRORS
// #else
// #define START_THREAD_TASK() /**/
// #define END_THREAD_TASK(name) /**/
// #define CATCH_RPC_METHOD_ERRORS
// #endif

using namespace boost::assign;
using nova::guest::apt::AptGuest;
using nova::guest::apt::AptGuestPtr;
using nova::guest::apt::AptMessageHandler;
using std::auto_ptr;
using nova::guest::backup::BackupManager;
using nova::guest::backup::BackupMessageHandler;
using nova::guest::backup::BackupRestoreManager;
using nova::guest::backup::BackupRestoreManagerPtr;
using nova::process::CommandList;
using nova::utils::CurlScope;
using boost::format;
using boost::optional;
using namespace nova;
using namespace nova::flags;
using namespace nova::guest;
using namespace nova::guest::diagnostics;
using namespace nova::guest::monitoring;
using namespace nova::db::mysql;
using namespace nova::guest::mysql;
using nova::utils::ThreadBasedJobRunner;
using namespace nova::rpc;
using std::string;
using nova::utils::Thread;
using std::vector;

// Begin anonymous namespace.
namespace {

const char PERIODIC_MESSAGE [] =
    "{"
    "    'method':'periodic_tasks',"
    "    'args':{}"
    "}";

/* Runs a single method. */
GuestOutput run_method(vector<MessageHandlerPtr> & handlers, GuestInput & input);

/* Runs a single method using a JSON string as input. */
void run_json_method(vector<MessageHandlerPtr> & handlers, const char * msg);

/* Handles receiving and processing messages. */
void message_loop(ResilentReceiver & receiver,
                  vector<MessageHandlerPtr> & handlers);

/* This is kind of a joke. There is no polite way to quit Sneaky. :p */
static const bool quit = false;

/**
 * Sends updates back to Reddwarf regarding the status of both
 * Sneaky Pete and the MySQL application.
 */
class PeriodicTasker : public Thread::Runner {

private:
    HeartBeat & heart_beat;
    const unsigned long periodic_interval;
    const unsigned long report_interval;
    MySqlAppStatusPtr status_updater;

public:
    PeriodicTasker(HeartBeat & heart_beat, MySqlAppStatusPtr status_updater,
                   unsigned long periodic_interval,
                   unsigned long report_interval)
      : heart_beat(heart_beat),
        periodic_interval(periodic_interval),
        report_interval(report_interval),
        status_updater(status_updater)
    {
    }

    virtual void operator()() {
        unsigned long next_periodic_task = periodic_interval;
        unsigned long next_reporting = report_interval;
        while(!quit) {
            unsigned long wait_time = next_periodic_task < next_reporting ?
                next_periodic_task : next_reporting;
            NOVA_LOG_TRACE("Waiting for %lu seconds...", wait_time);
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
            NOVA_LOG_TRACE("Running periodic tasks...");
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
    // This should evaluate to a single const char string.
    const char * const info = ("v" NOVA_GUEST_VERSION " " __DATE__ " " __TIME__);
    NOVA_LOG_INFO(" ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^");
    NOVA_LOG_INFO(" ^ ^ ^                                       ^");
    NOVA_LOG_INFO(" ^ '  '       -----REDDWARF-GUEST-AGENT----- ^");
    NOVA_LOG_INFO(" ^ \\`-'/        -------Sneaky--Pete-------   ^");
    string banner_text = str(format(
                  " ^   |__     %|+31| ^") % info);
    NOVA_LOG_INFO(banner_text.c_str());
    NOVA_LOG_INFO(" ^  /                         starting now...^");
    NOVA_LOG_INFO(" ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^");

    // Initialize MySQL libraries. This should be done before spawning
    // threads.
    MySqlApiScope mysql_api_scope;

    // Initialize curl.
    CurlScope scope;

    /* Create connection to Nova database. */
    MySqlConnectionWithDefaultDbPtr nova_db(
        new MySqlConnectionWithDefaultDb(
            flags.nova_sql_host(), flags.nova_sql_user(),
            flags.nova_sql_password(),
            flags.nova_sql_database()));

    /* Create JSON message handlers. */
    vector<MessageHandlerPtr> handlers;

    /* Create Apt Guest */
    AptGuestPtr apt_worker(new AptGuest(
        flags.apt_use_sudo(),
        flags.apt_self_package_name(),
        flags.apt_self_update_time_out()));
    MessageHandlerPtr handler_apt(new AptMessageHandler(apt_worker));
    handlers.push_back(handler_apt);

    /* Create MySQL updater. */
    MySqlAppStatusPtr mysql_status_updater(new MySqlAppStatus(
        nova_db,
        flags.nova_db_reconnect_wait_time(),
        flags.guest_id()));

    /* Create MySQL Guest. */
    MessageHandlerPtr handler_mysql(new MySqlMessageHandler());
    handlers.push_back(handler_mysql);

    Monitoring monitoring(flags.guest_id(),
                          flags.monitoring_agent_package_name(),
                          flags.monitoring_agent_config_file(),
                          flags.monitoring_agent_install_timeout());
    MessageHandlerPtr handler_monitoring_app(new MonitoringMessageHandler(
        apt_worker, monitoring));
    handlers.push_back(handler_monitoring_app);

    BackupRestoreManagerPtr backup_restore_manager(new BackupRestoreManager(
        flags.backup_restore_process_commands(),
        flags.backup_restore_delete_file_pattern(),
        flags.backup_restore_restore_directory(),
        flags.backup_restore_save_file_pattern(),
        flags.backup_restore_zlib_buffer_size()
    ));
    MySqlAppPtr mysqlApp(new MySqlApp(mysql_status_updater,
                                      backup_restore_manager,
                                      flags.mysql_state_change_wait_time(),
                                      flags.skip_install_for_prepare()));

    /* Create Volume Manager. */
    VolumeManagerPtr volumeManager(new VolumeManager(
        flags.volume_check_device_num_retries(),
        flags.volume_file_system_type(),
        flags.volume_format_options(),
        flags.volume_format_timeout(),
        flags.volume_mount_options()
    ));

    /** Sneaky Pete formats and mounts volumes based on the bool flag
      *'volume_format_and_mount' which is passed to MySqlAppMessageHandler which
      * uses the flag to decide weather to format/mount a volume during the
      * prepare call */
    /** TODO (joe.cruz) There has to be a better way of enabling sneaky pete to
      * format/mount a volume and to create the volume manager based on that.
      * I did this because currently flags can only be retrived from
      * receiver_daemon so the volumeManager has to be created here. */
    MessageHandlerPtr handler_mysql_app(
        new MySqlAppMessageHandler(mysqlApp,
                                   apt_worker,
                                   monitoring,
                                   flags.volume_format_and_mount(),
                                   volumeManager));
    handlers.push_back(handler_mysql_app);

    /* Create the Interrogator for the guest. */
    Interrogator interrogator;
    MessageHandlerPtr handler_interrogator(
        new InterrogatorMessageHandler(interrogator));
    handlers.push_back(handler_interrogator);

    /* Create job runner, but don't start it's thread until much later. */
    ThreadBasedJobRunner job_runner;

    /* Backup task */
    BackupManager backup(
                  nova_db,
                  job_runner,
                  flags.backup_process_commands(),
                  flags.backup_segment_max_size(),
                  flags.backup_swift_container(),
                  flags.backup_timeout(),
                  flags.backup_zlib_buffer_size());
    MessageHandlerPtr handler_backup(new BackupMessageHandler(backup));
    handlers.push_back(handler_backup);

    if (flags.register_dangerous_functions()) {
        NOVA_LOG_INFO("WARNING! Dangerous functions will be callable!");
        MessageHandlerPtr handler_diagnostics(
            new DiagnosticsMessageHandler(true));
        handlers.push_back(handler_diagnostics);
    }

    /* Set host value. */
    string actual_host = nova::guest::utils::get_host_name();
    string host = flags.host().get_value_or(actual_host.c_str());

    /* Create HeartBeat. */
    HeartBeat heart_beat(nova_db, flags.guest_id());

    /* Create AMQP connection. */
    string topic = str(format("guestagent.%s") % flags.guest_id());

    NOVA_LOG_INFO("Starting status thread...");
    PeriodicTasker tasker(heart_beat, mysql_status_updater,
                          flags.periodic_interval(), flags.report_interval());
    Thread statusThread(flags.status_thread_stack_size(), tasker);

    NOVA_LOG_INFO("Starting job thread...");
    Thread workerThread(flags.worker_thread_stack_size(), job_runner);

    // If a "message" is specified we just run it and quit. Otherwise,
    // it's Rabbit time.
    optional<const char *> message = flags.message();
    if (message) {
        run_json_method(handlers, message.get());
        // If a SP is being run with a message, it's possible it needs to run
        // in the job runner thread. If so, be sure to wait, or else this
        // thread will shut down the job runner before the request can even
        // begin.
        if (!job_runner.is_idle()) {
            NOVA_LOG_INFO("Sleeping to give the job runner thread a chance.");
            boost::this_thread::sleep(boost::posix_time::seconds(1));
        }
    } else {
        NOVA_LOG_INFO("Before we go further, lets chill out and take a nap.");
        // Before we go any further, lets all just chill out and take a nap.
        // TODO(tim.simpson) For reasons I can only assume relate to an even
        // worse bug I have been able to uncover, sleeping here keeps Sneaky
        // Pete from morphing into Fat Pete (inexplicable 60 MB virtual memory
        // increase).
        boost::this_thread::sleep(boost::posix_time::seconds(3));

        ResilentReceiver receiver(flags.rabbit_host(), flags.rabbit_port(),
                    flags.rabbit_userid(), flags.rabbit_password(),
                    flags.rabbit_client_memory(), topic.c_str(),
                    flags.control_exchange(),
                    flags.rabbit_reconnect_wait_time());

        message_loop(receiver, handlers);
    }

    // Gracefully kill the job runner.
    NOVA_LOG_INFO("Shutting down Sneaky Pete. Killing job runner.");
    job_runner.shutdown();
    NOVA_LOG_INFO(" ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^");
    NOVA_LOG_INFO(" ^           ^ ^                             ^");
    NOVA_LOG_INFO(" ^           -  -   < Good bye.)             ^");
    NOVA_LOG_INFO(" ^        /==/`-'\\                           ^");
    NOVA_LOG_INFO(" ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^");
}

void run_json_method(vector<MessageHandlerPtr> & handlers,
                     const char * msg) {
    JsonObject obj(msg);
    GuestInput input;
    Receiver::init_input_with_json(input, obj);
    run_method(handlers, input);
}

GuestOutput run_method(vector<MessageHandlerPtr> & handlers, GuestInput & input) {
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
        NOVA_LOG_ERROR("Error running method %s : %s",
                   input.method_name.c_str(), e.what());
        NOVA_LOG_ERROR(e.what());
        output.result.reset();
        output.failure = e.what();
    }
    #endif

    return output;
}

void message_loop(ResilentReceiver & receiver,
                  vector<MessageHandlerPtr> & handlers) {
    while(!quit) {
#ifndef _DEBUG
    try {
#endif
        GuestInput input = receiver.next_message();
        NOVA_LOG_INFO("method=%s", input.method_name.c_str());

        GuestOutput output(run_method(handlers, input));

        receiver.finish_message(output);
#ifndef _DEBUG
        } catch (const std::exception & e) {
            NOVA_LOG_ERROR("std::exception error: %s", e.what());
        } catch (...) {
            NOVA_LOG_ERROR("An exception ocurred of unknown origin!");
        }
#endif
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
        LogOptions log_options = log_options_from_flags(flags);
        LogApiScope log_api_scope(log_options);

        #ifndef _DEBUG
            try {
        #endif
            initialize_and_run(flags);
        #ifndef _DEBUG
            } catch (const std::exception & e) {
                NOVA_LOG_ERROR("Error: %s", e.what());
            }
        #endif

#ifndef _DEBUG
    } catch (const std::exception & e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
#endif
    return 0;
}
