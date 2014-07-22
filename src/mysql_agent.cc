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
#include "nova/guest/mysql/MySqlBackupManager.h"
#include "nova/guest/mysql/MySqlBackupRestoreManager.h"
#include "nova/guest/backup/BackupMessageHandler.h"
#include "nova/guest/monitoring/monitoring.h"
#include "nova/db/mysql.h"
#include <boost/foreach.hpp>
#include "nova/guest/GuestException.h"
#include <boost/lexical_cast.hpp>
#include <iostream>
#include <memory>
#include "nova/db/mysql.h"
#include "nova/guest/mysql/MySqlMessageHandler.h"
#include <boost/optional.hpp>
#include "nova/rpc/receiver.h"
#include <json/json.h>
#include "nova/Log.h"
#include "nova/guest/common/PrepareHandler.h"
#include <sstream>
#include <boost/thread.hpp>
#include "nova/utils/threads.h"
#include <boost/tuple/tuple.hpp>
#include "nova/guest/utils.h"
#include <vector>

#include "nova/guest/agent.h"


using namespace boost::assign;
using nova::guest::apt::AptGuest;
using nova::guest::apt::AptGuestPtr;
using nova::guest::apt::AptMessageHandler;
using std::auto_ptr;
using nova::backup::BackupManagerPtr;
using nova::guest::mysql::MySqlBackupManager;
using nova::guest::backup::BackupMessageHandler;
using nova::backup::BackupRestoreManagerPtr;
using nova::guest::mysql::MySqlBackupRestoreManager;
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
using nova::guest::common::PrepareHandler;
using nova::guest::common::PrepareHandlerPtr;
using nova::utils::ThreadBasedJobRunner;
using namespace nova::rpc;
using std::string;
using nova::utils::Thread;
using std::vector;

// Begin anonymous namespace.
namespace {


const char * const MOUNT_POINT="/var/lib/mysql";


class PeriodicTasks
{
public:
    PeriodicTasks(MonitoringManagerPtr monitoring_manager,
                  MySqlAppStatusPtr mysql_app_status)
    : monitoring_manager(monitoring_manager),
      mysql_app_status(mysql_app_status)
    {
    }

    void operator() ()
    {
        mysql_app_status->update();
        monitoring_manager->ensure_running();
    }

private:
    MonitoringManagerPtr monitoring_manager;
    MySqlAppStatusPtr mysql_app_status;
};


typedef boost::shared_ptr<PeriodicTasks> PeriodicTasksPtr;


struct Func {

    // Initialize curl.
    nova::utils::CurlScope scope;

    // Initialize MySQL libraries. This should be done before spawning
    // threads.
    nova::db::mysql::MySqlApiScope mysql_api_scope;

    static bool is_mysql_installed(std::list<std::string> package_list,
                                   AptGuestPtr & apt_worker) {
        BOOST_FOREACH(const auto & package_name, package_list) {
            if (apt_worker->version(package_name.c_str())) {
                return true;
            }
        }
        return false;
    }

    boost::tuple<std::vector<MessageHandlerPtr>, PeriodicTasksPtr>
        operator() (const FlagValues & flags,
                    ResilientSenderPtr sender,
                    ThreadBasedJobRunner & job_runner)
    {
        /* Create JSON message handlers. */
        vector<MessageHandlerPtr> handlers;

        /* Create Apt Guest */
        AptGuestPtr apt_worker(new AptGuest(AptGuest::from_flags(flags)));
        MessageHandlerPtr handler_apt(new AptMessageHandler(apt_worker));
        handlers.push_back(handler_apt);

        const auto package_list = flags.datastore_packages();

        /* Create MySQL updater. */
        MySqlAppStatusPtr mysql_status_updater(new MySqlAppStatus(
            sender, is_mysql_installed(package_list, apt_worker)));

        /* Create MySQL Guest. */
        MessageHandlerPtr handler_mysql(new MySqlMessageHandler());
        handlers.push_back(handler_mysql);

        MonitoringManagerPtr monitoring_manager(new MonitoringManager(
            MonitoringManager::from_flags(flags)));
        MessageHandlerPtr handler_monitoring_app(new MonitoringMessageHandler(
            apt_worker, monitoring_manager));
        handlers.push_back(handler_monitoring_app);

        BackupRestoreManagerPtr backup_restore_manager(
            MySqlBackupRestoreManager::from_flags(flags));
        MySqlAppPtr mysqlApp(new MySqlApp(mysql_status_updater,
                                          backup_restore_manager,
                                          flags.mysql_state_change_wait_time(),
                                          flags.skip_install_for_prepare()));

        /** Sneaky Pete formats and mounts volumes based on the bool flag
          *'volume_format_and_mount'.
          * If disabled a volume_manager null pointer is passed to the mysql
          * message handler. */
        VolumeManagerPtr volume_manager;
        if (flags.volume_format_and_mount()) {
          /** Create Volume Manager.
            * Reset null pointer to reference VolumeManager */
          volume_manager.reset(new VolumeManager(
            flags.volume_check_device_num_retries(),
            flags.volume_file_system_type(),
            flags.volume_format_options(),
            flags.volume_format_timeout(),
            MOUNT_POINT,
            flags.volume_mount_options()
          ));
        }

        /** TODO (joe.cruz) There has to be a better way of enabling sneaky pete to
          * format/mount a volume and to create the volume manager based on that.
          * I did this because currently flags can only be retrived from
          * receiver_daemon so the volume_manager has to be created here. */
          /* Register the prepare handler, which sets everything up. */
        PrepareHandlerPtr prepare_ptr(new PrepareHandler(
            mysqlApp, apt_worker, mysql_status_updater, volume_manager));
        MessageHandlerPtr handler_mysql_app(
            new MySqlAppMessageHandler(prepare_ptr,
                                       mysqlApp,
                                       apt_worker,
                                       monitoring_manager,
                                       volume_manager));
        handlers.push_back(handler_mysql_app);

        /* Create the Interrogator for the guest. */
        Interrogator interrogator(MOUNT_POINT);
        MessageHandlerPtr handler_interrogator(
            new InterrogatorMessageHandler(interrogator));
        handlers.push_back(handler_interrogator);

        /* Backup task */
        BackupManagerPtr backup = MySqlBackupManager::from_flags(
            flags, sender, job_runner, interrogator);
        MessageHandlerPtr handler_backup(new BackupMessageHandler(backup));
        handlers.push_back(handler_backup);

        if (flags.register_dangerous_functions()) {
            NOVA_LOG_INFO("WARNING! Dangerous functions will be callable!");
            MessageHandlerPtr handler_diagnostics(
                new DiagnosticsMessageHandler(true));
            handlers.push_back(handler_diagnostics);
        }

        PeriodicTasksPtr updater(new PeriodicTasks(
          monitoring_manager, mysql_status_updater
          ));

        return boost::make_tuple(handlers, updater);
    }

};

} // end anonymous namespace


int main(int argc, char* argv[]) {
    return ::nova::guest::agent::execute_main<Func, PeriodicTasksPtr>(
        "MySQL Edition", argc, argv);
}
