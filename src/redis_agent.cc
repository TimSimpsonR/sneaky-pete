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
#include "nova/backup/BackupManager.h"
#include "nova/guest/backup/BackupMessageHandler.h"
#include "nova/backup/BackupRestore.h"
#include "nova/db/mysql.h"
#include <boost/foreach.hpp>
#include "nova/guest/GuestException.h"
#include <boost/lexical_cast.hpp>
#include <iostream>
#include <memory>
#include "nova/db/mysql.h"
#include "nova/redis/RedisAppStatus.h"
#include "nova/guest/redis/message_handler.h"
#include <boost/optional.hpp>
#include "nova/guest/common/PrepareHandler.h"
#include "nova/redis/RedisApp.h"
#include "nova/rpc/receiver.h"
#include <json/json.h>
#include "nova/Log.h"
#include <sstream>
#include <boost/thread.hpp>
#include "nova/utils/threads.h"
#include <boost/tuple/tuple.hpp>
#include "nova/guest/utils.h"
#include <vector>
#include "nova/guest/agent.h"
#include <unistd.h>

using nova::guest::agent::execute_main;
using namespace boost::assign;
using nova::guest::apt::AptGuest;
using nova::guest::apt::AptGuestPtr;
using nova::guest::apt::AptMessageHandler;
using nova::redis::RedisAppStatusPtr;
using nova::redis::RedisAppStatus;
using std::auto_ptr;
using nova::backup::BackupManager;
using nova::guest::backup::BackupMessageHandler;
using nova::backup::BackupRestoreManager;
using nova::backup::BackupRestoreManagerPtr;
using nova::process::CommandList;
using nova::utils::CurlScope;
using boost::format;
using boost::optional;
using namespace nova;
using namespace nova::flags;
using namespace nova::guest;
using namespace nova::guest::diagnostics;
using nova::guest::common::PrepareHandler;
using nova::guest::common::PrepareHandlerPtr;
using nova::redis::RedisApp;
using nova::redis::RedisAppPtr;
using nova::redis::RedisAppStatus;
using nova::redis::RedisAppStatusPtr;
using nova::rpc::ResilientSenderPtr;
using namespace nova::rpc;
using std::string;
using nova::utils::Thread;
using nova::utils::ThreadBasedJobRunner;
using nova::VolumeManager;
using std::vector;

namespace
{


const char * const MOUNT_POINT="/";


class PeriodicTasks
{
    public:
        PeriodicTasks(RedisAppStatusPtr app_status):
                      app_status(app_status)
    {
    }

    void operator() ()
    {
        app_status->update();
    }

    private:
        RedisAppStatusPtr app_status;
};

bool is_redis_installed()
{
    if(access("/usr/bin/redis-server", F_OK) == -1)
    {
        return false;
    }
    else
    {
        return true;
    }
}

typedef boost::shared_ptr<PeriodicTasks> PeriodicTasksPtr;

struct Func {
    boost::tuple<vector<MessageHandlerPtr>, PeriodicTasksPtr>
        operator() (const FlagValues & flags,
                    ResilientSenderPtr & sender,
                    ThreadBasedJobRunner & job_runner)
    {
        /* Create JSON message handlers. */
        vector<MessageHandlerPtr> handlers;
        /* Create Apt Guest */
        AptGuestPtr apt_worker(new AptGuest(AptGuest::from_flags(flags)));
        MessageHandlerPtr handler_apt(new AptMessageHandler(apt_worker));
        handlers.push_back(handler_apt);

        RedisAppStatusPtr app_status(new RedisAppStatus(sender,
                                                        is_redis_installed()));
        RedisAppPtr app(new RedisApp(app_status,
                                     flags.redis_state_change_wait_time()));
        VolumeManagerPtr volume_manager;  // Intentionally leave this null.
        PrepareHandlerPtr prepare_ptr(new PrepareHandler(
            app, apt_worker, app_status, volume_manager));
        RedisMessageHandler t(app, prepare_ptr, app_status);
        MessageHandlerPtr handler_redis(new RedisMessageHandler(
            app, prepare_ptr, app_status));

        Interrogator interrogator(MOUNT_POINT);
        MessageHandlerPtr handler_interrogator(
            new InterrogatorMessageHandler(interrogator));
        handlers.push_back(handler_interrogator);

        handlers.push_back(handler_redis);
        PeriodicTasksPtr tasks(new PeriodicTasks(app_status));
        return boost::make_tuple(handlers, tasks);
    }

};

} // end anonymous namespace


int main(int argc, char* argv[]) {
    return execute_main<Func, PeriodicTasksPtr>("Redis Edition", argc, argv);
}
