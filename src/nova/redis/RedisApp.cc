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

using nova::guest::backup::BackupRestoreInfo;
using boost::format;
using nova::utils::io::is_file;
using std::ofstream;
using boost::optional;
using nova::redis::RedisAppStatusPtr;
using nova::process::shell;
using std::string;


// Macros are bad, but better than duplicating these two lines everywhere.
#define THROW_PASSWORD_ERROR(msg) { \
        NOVA_LOG_ERROR(msg); \
        throw RedisException(RedisException::CHANGE_PASSWORD_ERROR); }

#define THROW_PREPARE_ERROR(msg) { \
        NOVA_LOG_ERROR(msg); \
        throw RedisException(RedisException::PREPARE_ERROR); }


namespace nova { namespace redis {

namespace {
    std::string get_uuid()
    {
        std::stringstream ss;
        boost::uuids::uuid uuid = boost::uuids::random_generator()();
        ss << uuid;
        return ss.str();
    }

    string read_local_conf_omit_password() {
        string file_contents;
        NOVA_LOG_INFO("Opening local.conf for reading/writing");
        {
            std::ifstream local_conf;
            local_conf.open("/etc/redis/conf.d/local.conf");
            if (!local_conf.good()) {
                NOVA_LOG_INFO("Getting rid of the password entry");
                string tmp;
                while(!local_conf.eof()) {
                    if (!local_conf.good()) {
                        NOVA_LOG_ERROR("File error reading local.conf.");
                        throw RedisException(RedisException::LOCAL_CONF_READ_ERROR);
                    }
                    getline(local_conf, tmp);
                    if (string::npos == tmp.find("requirepass")) {
                        if (tmp != "\n") {
                            file_contents.append(tmp);
                            file_contents.append("\n");
                        }
                    }
                }
            }
            else {
                NOVA_LOG_ERROR("Unable to read local.conf.");
                throw RedisException(RedisException::LOCAL_CONF_READ_ERROR);
            }
        }
        return file_contents;
    }

    void write_local_conf(const string & contents) {
        std::ofstream local_conf;
        local_conf.open("/etc/redis/conf.d/local.conf");
        if (local_conf.good()) {
            local_conf << contents;
            local_conf.close();
        }
        else {
            NOVA_LOG_ERROR("Unable to open local.conf for writing");
            throw RedisException(RedisException::LOCAL_CONF_WRITE_ERROR);
        }
    }
}

RedisApp::RedisApp(RedisAppStatusPtr app_status)
:   app_status(app_status) {
}

RedisApp::~RedisApp() {
}



void RedisApp::change_password(const string & password) {
    NOVA_LOG_INFO("Starting change_passwords call");
    shell("sudo chmod -R 777 /etc/redis/conf.d");
    string file_contents = read_local_conf_omit_password();
    file_contents.append(str(format("requirepass %s\n") % password));

    shell("sudo rm /etc/redis/conf.d/local.conf");
    shell("touch /etc/redis/conf.d/local.conf");
    write_local_conf(file_contents);
    NOVA_LOG_INFO("chmoding /etc/redis/conf.d to 755");
    if (system("sudo chmod -R 755 /etc/redis/conf.d") == -1) {
        //TODO(tim.simpson): Should we throw if chmod doesn't work here?
        NOVA_LOG_ERROR("Unable to return conf.d to previous permissions");
    }
    NOVA_LOG_INFO("chmowning /etc/redis/conf.d to 755");
    if (system("sudo chown -R root:root /etc/redis/conf.d") == -1) {
        //TODO(tim.simpson): Should we throw if chown doesn't work here?
        NOVA_LOG_ERROR("Unable to change ownership of conf.d to root:root");
    }
    NOVA_LOG_INFO("Connecting to redis");
    nova::redis::Client client;
    client.config_set("requirepass", password);
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

    shell("sudo rm /etc/redis/redis.conf");
    shell("sudo chmod -R 777 /etc/redis");

    NOVA_LOG_INFO("Creating new config file.");
    {
        ofstream fd;
        fd.open("/etc/redis/redis.conf");
        NOVA_LOG_INFO("Writing config contents.");
        NOVA_LOG_DEBUG(config_contents.c_str());
        if (fd.good())
        {
            fd << config_contents;
            fd.close();
        }
        else
        {
            THROW_PREPARE_ERROR("Couldn't open config file.");
        }
    }
    shell("sudo chmod 644 /etc/redis/redis.conf");
    shell("mkdir /etc/redis/conf.d");
    if (is_file("/etc/redis/conf.d/local.conf")) {
        shell("rm /etc/redis/conf.d/local.conf");
    }
    NOVA_LOG_INFO("Opening /etc/redis/conf.d/local.conf for writing");
    {
        ofstream fd;
        fd.open("/etc/redis/conf.d/local.conf");
        if (fd.is_open())
        {
            const string local_config = str(format(
                "requirepass %1%\n"
                "rename-command CONFIG %2%\n"
                "rename-command MONITOR %3%")
                % root_password % get_uuid() % get_uuid());
            fd << local_config;
            fd.close();
            fd.clear();
        }
        else
        {
            THROW_PREPARE_ERROR("Unable to open /etc/redis/conf.d/local.conf");
        }
    }
    shell("sudo chmod 644 /etc/redis/conf.d/local.conf");
    shell("sudo chmod 755 /etc/redis");
    if (system("sudo chmod 755 /etc/redis/conf.d") == -1)
    {
        THROW_PREPARE_ERROR("Unable to chmod -R /etc/redis/conf.d to 755");
    }
    shell("sudo chown -R root:root /etc/redis");
    NOVA_LOG_INFO("Connecting to redis instance.");
    nova::redis::Client client = nova::redis::Client();
    NOVA_LOG_INFO("Stopping redis instance.");
    if (client.control->stop() != 0)
    {
        NOVA_LOG_INFO("Unable to stop redis instance.");
        //TODO(tim.simpson): If we can't stop, is this just ok?
    }
    NOVA_LOG_INFO("Starting redis instance.");
    if (client.control->start() != 0)
    {
        THROW_PREPARE_ERROR("Unable to start redis instance!");
    }
}


void RedisApp::restart() {
    NOVA_LOG_INFO("Entering restart call.");
    Client client;
    NOVA_LOG_INFO("Stopping redis instance.");
    if (client.control->stop() != 0)
    {
        NOVA_LOG_ERROR("Unable to stop redis instance!");
        throw RedisException(RedisException::COULD_NOT_STOP);
    }
    NOVA_LOG_INFO("Starting redis instance.");
    if (client.control->start() != 0)
    {
        NOVA_LOG_ERROR("Unable to start redis instance!");
        throw RedisException(RedisException::COULD_NOT_START);
    }
    NOVA_LOG_INFO("Redis instance restarted successfully.");
}


void RedisApp::start_with_conf_changes(const std::string & config_contents) {
    NOVA_LOG_INFO("Entering start_db_with_conf_changes call.");
    nova::redis::Client client;
    if (!client.config->write_new_config(config_contents))
    {
        NOVA_LOG_ERROR("Unable to write new config.");
        throw RedisException(RedisException::UNABLE_TO_WRITE_CONFIG);
    }
    if (client.control->start() != 0)
    {
        NOVA_LOG_ERROR("Unable to start redis instance!");
        throw RedisException(RedisException::COULD_NOT_START);
    }
    NOVA_LOG_INFO("Redis instance started.");
}


void RedisApp::stop() {
    NOVA_LOG_INFO("Entering stop_db call.");
    nova::redis::Client client;
    NOVA_LOG_INFO("Stopping redis instance.");
    if (client.control->stop() != 0)
    {
        NOVA_LOG_ERROR("Unable to stop redis instance!");
        throw RedisException(RedisException::COULD_NOT_STOP);
    }
    NOVA_LOG_INFO("Redis instance stopped.");
}

} }  // end namespace
