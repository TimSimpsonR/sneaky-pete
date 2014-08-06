#include "pch.hpp"
#include "RedisApp.h"
#include "RedisException.h"
#include "nova/guest/redis/client.h"
#include "nova/utils/io.h"
#include <iostream>
#include <boost/assign/list_of.hpp>
#include <boost/assign/std/list.hpp>
#include "nova/process.h"
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

using namespace boost::assign;
using nova::backup::BackupRestoreInfo;
using nova::datastores::DatastoreAppPtr;
using nova::datastores::DatastoreStatusPtr;
using nova::process::execute;
using nova::process::force_kill;
using boost::format;
using nova::utils::io::is_file;
using std::ofstream;
using boost::optional;
using nova::process::ProcessException;
using nova::redis::RedisAppStatusPtr;
using nova::process::shell;
using std::string;
using std::stringstream;

// Use a macro so we log the line number from this file.
#define SHELL(cmd) { \
        NOVA_LOG_INFO(cmd); \
        shell(cmd, false); \
    }

// Macros are bad, but better than duplicating these two lines everywhere.
#define THROW_PASSWORD_ERROR(msg) { \
        NOVA_LOG_ERROR(msg); \
        throw RedisException(RedisException::CHANGE_PASSWORD_ERROR); }

#define THROW_PREPARE_ERROR(msg) { \
        NOVA_LOG_ERROR(msg); \
        throw RedisException(RedisException::PREPARE_ERROR); }


namespace nova { namespace redis {

RedisApp::RedisApp(RedisAppStatusPtr status,
                   const int state_change_wait_time)
:   DatastoreApp("redis-server", status, state_change_wait_time)
{
}

RedisApp::~RedisApp() {
}

void RedisApp::change_password(const string & password) {
    NOVA_LOG_INFO("Connecting to redis");
    nova::redis::Client client;
    client.config_set("requirepass", password);
    client.config_rewrite();
}

void RedisApp::specific_start_app_method() {
    stringstream out;
    execute(out,
            list_of("/usr/bin/sudo")("/etc/init.d/redis-server")("start"),
            state_change_wait_time);
}

void RedisApp::specific_stop_app_method() {
    try {
        stringstream out;
        execute(out,
                list_of("/usr/bin/sudo")("/etc/init.d/redis-server")("stop"));
        //TODO(tim.simpson): Make sure we need to kill it. Must it truly pay
        //                   the ultimate price?
    } catch(const ProcessException & pe) {
        NOVA_LOG_ERROR("Couldn't stop redis-server the nice way. Now it "
                       "must die.");
        auto pid = RedisAppStatus::get_pid();
        if (!pid) {
            NOVA_LOG_ERROR("No pid for Redis found!");
            throw RedisException(RedisException::COULD_NOT_STOP);
        } else {
            NOVA_LOG_ERROR("Discovered pid, killing...");
            force_kill(pid.get());
        }
    }
}

void RedisApp::prepare(const optional<string> & json_root_password,
                       const string & config_contents,
                       const optional<string> & overrides,
                       optional<BackupRestoreInfo> restore)
{
    if (!json_root_password) {
        NOVA_LOG_ERROR("Missing root password!");
        throw RedisException(RedisException::MISSING_ROOT_PASSWORD);
    }
    const string root_password = json_root_password.get();

    SHELL("sudo rm /etc/redis/redis.conf");
    SHELL("sudo chmod -R 777 /etc/redis");

    write_config(config_contents, root_password);


    NOVA_LOG_INFO("Starting to muck with Redis's config file.");
    SHELL("mkdir -p /etc/redis/conf.d");
    NOVA_LOG_INFO("Checking for Redis file.");
    if (is_file("/etc/redis/conf.d/local.conf")) {
        SHELL("rm /etc/redis/conf.d/local.conf");
    }

    NOVA_LOG_INFO("Opening /etc/redis/conf.d/local.conf for writing");
    {
        ofstream fd;
        fd.open("/etc/redis/conf.d/local.conf");
        if (fd.is_open())
        {
            Config config;
            const string local_conf = str(format(
                        "maxmemory %1%\n"
                        "dir %2%\n"
                        "pidfile %3%\n"
                        "dbfilename %4%\n"
                        "daemonize yes\n"
                        "port %5%\n"
                        "logfile %6%\n"
                        "appendfilename %7%")
                        % config.get_max_memory()
                        % config.get_db_dir()
                        % config.get_pidfile()
                        % config.get_db_filename()
                        % config.get_port()
                        % config.get_log_file()
                        % config.get_append_filename());

            fd << local_conf;
            fd.close();
            fd.clear();
        }
        else
        {
            THROW_PREPARE_ERROR("Unable to open /etc/redis/conf.d/local.conf");
        }
    }
    SHELL("sudo chmod 644 /etc/redis/conf.d/local.conf");
    SHELL("sudo chmod 755 /etc/redis");
    if (system("sudo chmod 755 /etc/redis/conf.d") == -1)
    {
        THROW_PREPARE_ERROR("Unable to chmod /etc/redis/conf.d to 755");
    }
    SHELL("sudo chown -R redis:redis /etc/redis");
    NOVA_LOG_INFO("Setting up Redis to start on boot.");
    start_on_boot.setup_defaults();
    start_on_boot.enable_or_throw();
    NOVA_LOG_INFO("Connecting to redis instance.");
    if (status->is_running()) {
        NOVA_LOG_INFO("Stopping redis instance.");
        internal_stop(false);
    }
    NOVA_LOG_INFO("Starting redis instance.");
    internal_start(false);

    NOVA_LOG_INFO("Making sure we can connect to Redis...");
    nova::redis::Client client;
}

void RedisApp::reset_configuration(const std::string & config_contents) {
    Config config;
    const string root_password = config.get_require_pass();
    write_config(config_contents, root_password);
}

void RedisApp::start_with_conf_changes(const std::string & config_contents) {
    //TODO (cweid): I need to add in HA persistance to the upgrade calls.
    // Will add in the Am.
    NOVA_LOG_INFO("Entering start_db_with_conf_changes call.");
    Config config;
    const string root_password = config.get_require_pass();
    reset_configuration(config_contents);
    internal_start(true);
    NOVA_LOG_INFO("Redis instance started.");
}

void RedisApp::write_config(const string & config_contents,
                            const string & root_password) {
    NOVA_LOG_INFO("Creating new config file.");
    {
        ofstream fd;
        fd.open("/tmp/redis.conf");
        NOVA_LOG_INFO("Writing config contents.");
        NOVA_LOG_DEBUG(config_contents.c_str());
        if (fd.good())
        {
            const string requirepass = str(format("requirepass %1%")
                % root_password);
            fd << requirepass << std::endl << config_contents;
            fd.close();
        }
        else
        {
            THROW_PREPARE_ERROR("Couldn't open config file.");
        }
    }
    SHELL("sudo cp /tmp/redis.conf /etc/redis/redis.conf");
    SHELL("sudo chmod 644 /etc/redis/redis.conf");
}

void RedisApp::enable_ssl(const string & ca_certificate, const string & private_key, const string & public_key) {

    return;

}

} }  // end namespace
