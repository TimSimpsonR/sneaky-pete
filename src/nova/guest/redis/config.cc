#include "pch.hpp"
#include "config.h"
#include <boost/lexical_cast.hpp>
#include <boost/foreach.hpp>
#include <boost/format.hpp>
#include <map>
#include <vector>
#include <fstream>
#include <string>
#include <cstdlib>
#include <fstream>
#include <stdio.h>
#include "constants.h"
#include "nova/Log.h"

using nova::Log;



namespace nova { namespace redis {


std::string Config::_get_string_value(std::string key)
{
    if (_options.count(key) > 0)
    {
        return _options[key];
    }
    return "";
}

int Config::_get_int_value(std::string key)
{
    if (_options.count(key) > 0)
    {
        return boost::lexical_cast<int>(_options[key]);
    }
    return INT_NULL;
}

bool Config::_get_bool_value(std::string key)
{
    if (_options.count(key) > 0)
    {
        if (_options[key] == TYPE_TRUE)
        {
            return true;
        }
        if (_options[key] == TYPE_FALSE)
        {
            return false;
        }
    }
    return NULL;
}

Config::Config(std::string config) : _redis_config(config)
{
    std::string line;
    std::string value;
    std::ifstream rconfig(_redis_config.c_str());
    if (!rconfig.is_open())
    {
        return;
    }
    while(std::getline(rconfig, line))
    {
        if (line.length() <= 1)
        {
            continue;
        }
        std::istringstream iss(line);
        iss >> value;
        if(value == INCLUDE_FILE)
        {
            iss >> value;
            _include.push_back(value);
        }
        else if(value == BIND_ADDR)
        {
            while(true)
            {
                if(iss.rdbuf()->in_avail() == 0)
                {
                    break;
                }
                iss >> value;
                _bind_addrs.push_back(value);
            }
        }
        else if(value == SAVE)
        {
            std::map<std::string, std::string> save_map;
            iss >> value;
            save_map[SAVE_SECONDS] = value;
            iss >> value;
            save_map[SAVE_CHANGES] = value;
            _save.push_back(save_map);
        }
        else if(value == SLAVE_OF)
        {
            iss >> value;
            _slave_of[MASTER_IP] = value;
            iss >> value;
            _slave_of[MASTER_PORT] = value;
        }
        else if(value == RENAME_COMMAND)
        {
            std::map<std::string, std::string> rename_command;
            iss >> value;
            rename_command[RENAMED_COMMAND] = value;
            iss >> value;
            rename_command[NEW_COMMAND_NAME] = value;
            _renamed_commands.push_back(rename_command);
        }
        else if(value == CLIENT_OUTPUT_BUFFER_LIMIT)
        {
            std::map<std::string, std::string> buff_limit;
            iss >> value;
            buff_limit[CLIENT_BUFFER_LIMIT_CLASS] = value;
            iss >> value;
            buff_limit[CLIENT_BUFFER_LIMIT_HARD_LIMIT] = value;
            iss >> value;
            buff_limit[CLIENT_BUFFER_LIMIT_SOFT_LIMIT] = value;
            iss >> value;
            buff_limit[CLIENT_BUFFER_LIMIT_SOFT_SECONDS] = value;
            _client_output_buffer_limit.push_back(buff_limit);
        }
        else
        {
            std::string option_value;
            iss >> option_value;
            if (value == REQUIRE_PASS)
            {
                NOVA_LOG_INFO("Found pass");
                NOVA_LOG_INFO(option_value.c_str());
            }
            _options[value] = option_value;
        }
    }
    rconfig.close();

}
std::vector<std::string> Config::get_include_files()
{
    return _include;
}

void Config::set_include_files(std::string value,
                          bool clear_existing_values)
{
    if (clear_existing_values)
    {
        _include.clear();
    }
    _include.push_back(value);
}

bool Config::get_daemonize()
{
    return _get_bool_value(DAEMONIZE);
}

void Config::set_daemonize(bool value)
{
    if(value)
    {
        _options[DAEMONIZE] = "yes";
    }
    else
    {
        _options[DAEMONIZE] = "no";
    }
}

std::string Config::get_pidfile()
{
    return _get_string_value(PIDFILE);
}

void Config::set_pidfile(std::string value)
{
    _options[PIDFILE] = value;
}

int Config::get_port()
{
    return _get_int_value(PORT);
}

void Config::set_port(int value)
{
    _options[PORT] = boost::lexical_cast<std::string>(value);
}

int Config::get_tcp_backlog()
{
    return _get_int_value(TCP_BACKLOG);
}

void Config::set_tcp_backlog(int value)
{
    _options[TCP_BACKLOG] = boost::lexical_cast<std::string>(value);
}

std::vector<std::string> Config::get_bind_addrs()
{
    return _bind_addrs;
}

void Config::set_bind_addrs(std::string value,
                            bool clear_existing_values)
{
    if (clear_existing_values)
    {
        _bind_addrs.clear();
    }
    _bind_addrs.push_back(value);
}

std::string Config::get_unix_socket()
{
    return _get_string_value(UNIX_SOCKET);
}

void Config::set_unix_socket(std::string value)
{
    _options[UNIX_SOCKET] = value;
}

int Config::get_unix_socket_permission()
{
    return _get_int_value(UNIX_SOCKET_PERMISSION);
}

void Config::set_unix_socket_permission(int value)
{
    _options[UNIX_SOCKET_PERMISSION] = boost::lexical_cast<std::string>(value);
}

int Config::get_tcp_keepalive()
{
    return _get_int_value(TCP_KEEPALIVE);
}

void Config::set_tcp_keepalive(int value)
{
    _options[TCP_KEEPALIVE] = boost::lexical_cast<std::string>(value);
}

std::string Config::get_log_level()
{
    return _get_string_value(LOG_LEVEL);
}

void Config::set_log_level(std::string value)
{
    _options[LOG_LEVEL] = value;
}

std::string Config::get_log_file()
{
    return _get_string_value(LOG_FILE);
}

void Config::set_log_file(std::string value)
{
    _options[LOG_FILE] = value;
}

bool Config::get_syslog_enabled()
{
    return _get_bool_value(SYSLOG);
}

void Config:: set_syslog_enabled(bool value)
{
    if (value)
    {
        _options[SYSLOG] = "yes";
    }
    else
    {
        _options[SYSLOG] = "no";
    }
}

std::string Config::get_syslog_ident()
{
    return _get_string_value(SYSLOG_IDENT);
}

void Config::set_syslog_ident(std::string value)
{
    _options[SYSLOG_IDENT] = value;
}

std::string Config::get_syslog_facility()
{
    return _get_string_value(SYSLOG_FACILITY);
}

void Config::set_syslog_facility(std::string value)
{
    _options[SYSLOG_FACILITY];
}

int Config::get_databases()
{
    return _get_int_value(DATABASES);
}

void Config::set_databases(int value)
{
    _options[DATABASES] = boost::lexical_cast<std::string>(value);
}

std::vector<std::map <std::string, std::string> > Config::get_save_intervals()
{
    return _save;
}

void Config::set_save_intervals(std::string changes, std::string seconds,
                                bool clear_existing_values)
{
    if (clear_existing_values)
    {
        _save.clear();
    }
    std::map <std::string, std::string> data;
    data[SAVE_SECONDS] = seconds;
    data[SAVE_CHANGES] = changes;
    _save.push_back(data);
}

bool Config::get_stop_writes_on_bgsave_error()
{
    return _get_bool_value(STOP_WRITES_ON_BGSAVE_ERROR);
}

void Config::set_stop_writes_on_bgsave_error(bool value)
{
    if (value)
    {
        _options[STOP_WRITES_ON_BGSAVE_ERROR] = "yes";
    }
    else
    {
        _options[STOP_WRITES_ON_BGSAVE_ERROR] = "no";
    }
}

bool Config::get_rdb_compression()
{
    return _get_bool_value(RDB_COMPRESSION);
}

void Config::set_rdb_compression(bool value)
{
    if (value)
    {
        _options[RDB_COMPRESSION] = "yes";
    }
    else
    {
        _options[RDB_COMPRESSION] = "no";
    }
}

bool Config::get_rdb_checksum()
{
    return _get_bool_value(RDB_CHECKSUM);
}

void Config::set_rdb_checksum(bool value)
{
    if (value)
    {
        _options[RDB_CHECKSUM] = "yes";
    }
    else
    {
        _options[RDB_CHECKSUM] = "no";
    }
}

std::string Config::get_db_filename()
{
    return _get_string_value(DB_FILENAME);
}

void Config::set_db_filename(std::string value)
{
    _options[DB_FILENAME] = value;
}

std::string Config::get_db_dir()
{
    return _get_string_value(DB_DIR);
}

void Config::set_db_dir(std::string value)
{
    _options[DB_DIR] = value;
}

std::map<std::string, std::string> Config::get_slave_of()
{
    return _slave_of;
}

void Config::set_slave_of(std::string master_ip,
                          std::string master_port)
{
    _slave_of[MASTER_IP] = master_ip;
    _slave_of[MASTER_PORT] = master_port;
}

std::string Config::get_master_auth()
{
    return _get_string_value(MASTER_AUTH);
}

void Config::set_master_auth(std::string value)
{
    _options[MASTER_AUTH];
}

bool Config::get_slave_serve_stale_data()
{
    return _get_bool_value(SLAVE_SERVE_STALE_DATA);
}

void Config::set_slave_serve_stale_data(bool value)
{
    if (value)
    {
        _options[SLAVE_SERVE_STALE_DATA] = "yes";
    }
    else
    {
        _options[SLAVE_SERVE_STALE_DATA] = "no";
    }
}

bool Config::get_slave_read_only()
{
    return _get_bool_value(SLAVE_READ_ONLY);
}

void Config::set_slave_read_only(bool value)
{
    if (value)
    {
        _options[SLAVE_READ_ONLY] = "yes";
    }
    else
    {
        _options[SLAVE_READ_ONLY] = "no";
    }
}

int Config::get_repl_ping_slave_period()
{
    return _get_int_value(REPL_PING_SLAVE_PERIOD);
}

void Config::set_repl_ping_slave_period(int value)
{
    _options[REPL_PING_SLAVE_PERIOD] = boost::lexical_cast<std::string>(value);
}

int Config::get_repl_timeout()
{
    return _get_int_value(REPL_TIMEOUT);
}

void Config::set_repl_timeout(int value)
{
    _options[REPL_TIMEOUT] = boost::lexical_cast<std::string>(value);
}

bool Config::get_repl_disable_tcp_nodelay()
{
    return _get_bool_value(REPL_DISABLE_TCP_NODELAY);
}

void Config::set_repl_disable_tcp_nodelay(bool value)
{
    if (value)
    {
        _options[REPL_DISABLE_TCP_NODELAY] = "yes";
    }
    else
    {
        _options[REPL_DISABLE_TCP_NODELAY] = "no";
    }
}

std::string Config::get_repl_backlog_size()
{
    return _get_string_value(REPL_BACKLOG_SIZE);
}

void Config::set_repl_backlog_size(std::string value)
{
    _options[REPL_BACKLOG_SIZE] = value;
}

int Config::get_repl_backlog_ttl()
{
    return _get_int_value(REPL_BACKLOG_TTL);
}

void Config::set_repl_backlog_ttl(int value)
{
    _options[REPL_BACKLOG_TTL] = boost::lexical_cast<std::string>(value);
}

int Config::get_slave_priority()
{
    return _get_int_value(SLAVE_PRIORITY);
}

void Config::set_slave_priority(int value)
{
    _options[SLAVE_PRIORITY] = boost::lexical_cast<std::string>(value);
}

int Config::get_min_slaves_to_write()
{
    return _get_int_value(MIN_SLAVES_TO_WRITE);
}

void Config::set_min_slaves_to_write(int value)
{
    _options[MIN_SLAVES_TO_WRITE] = boost::lexical_cast<std::string>(value);
}

int Config::get_min_slaves_max_lag()
{
    return _get_int_value(MIN_SLAVES_MAX_LAG);
}

void Config::set_min_slaves_max_lag(int value)
{
    _options[MIN_SLAVES_MAX_LAG] = boost::lexical_cast<std::string>(value);
}

std::string Config::get_require_pass()
{
    return _get_string_value(REQUIRE_PASS);
}

void Config::set_require_pass(std::string value)
{
    _options[REQUIRE_PASS] = value;
}

std::vector<std::map <std::string, std::string> >
    Config::get_renamed_commands()
{
    return _renamed_commands;
}

void Config::set_renamed_commands(std::string command,
                                  std::string renamed_command,
                                  bool clear_existing_values)
{
    if (clear_existing_values)
    {
        _renamed_commands.clear();
    }
    std::map <std::string, std::string> command_map;
    command_map[NEW_COMMAND_NAME] = renamed_command;
    command_map[RENAMED_COMMAND] = command;
    _renamed_commands.push_back(command_map);
}

int Config::get_max_clients()
{
    return _get_int_value(MAX_CLIENTS);
}

void Config::set_max_clients(int value)
{
    _options[MAX_CLIENTS] = boost::lexical_cast<std::string>(value);
}

std::string Config::get_max_memory()
{
    return _get_string_value(MAX_MEMORY);
}

void Config::set_max_memory(std::string value)
{
    _options[MAX_MEMORY] = value;
}

std::string Config::get_max_memory_policy()
{
    return _get_string_value(MAX_MEMORY_POLICY);
}

void Config::set_max_memory_policy(std::string value)
{
    _options[MAX_MEMORY_POLICY] = value;
}

int Config::get_max_memory_samples()
{
    return _get_int_value(MAX_MEMORY_SAMPLES);
}

void Config::set_max_memory_samples(int value)
{
    _options[MAX_MEMORY_SAMPLES] = boost::lexical_cast<std::string>(value);
}

bool Config::get_append_only()
{
    return _get_bool_value(APPEND_ONLY);
}

void Config::set_append_only(bool value)
{
    if (value)
    {
        _options[APPEND_ONLY] = "yes";
    }
    else
    {
        _options[APPEND_ONLY] = "no";
    }
}

std::string Config::get_append_filename()
{
    return _get_string_value(APPEND_FILENAME);
}

void Config::set_append_filename(std::string value)
{
    _options[APPEND_FILENAME] = value;
}

std::string Config::get_append_fsync()
{
    return _get_string_value(APPEND_FSYNC);
}

void Config::set_append_fsync(std::string value)
{
    _options[APPEND_FSYNC] = value;
}

bool Config::get_no_append_fsync_on_rewrite()
{
    return _get_bool_value(NO_APPEND_FSYNC_ON_REWRITE);
}

void Config::set_no_append_fsync_on_rewrite(bool value)
{
    if (value)
    {
        _options[NO_APPEND_FSYNC_ON_REWRITE] = "yes";
    }
    else
    {
        _options[NO_APPEND_FSYNC_ON_REWRITE] = "no";
    }
}

int Config::get_auto_aof_rewrite_percentage()
{
    return _get_int_value(AUTO_AOF_REWRITE_PERCENTAGE);
}

void Config::set_auto_aof_rewrite_percentage(int value)
{
    _options[AUTO_AOF_REWRITE_PERCENTAGE] =
        boost::lexical_cast<std::string>(value);
}

std::string Config::get_auto_aof_rewrite_min_size()
{
    return _get_string_value(AUTO_AOF_REWRITE_MIN_SIZE);
}

void Config::set_auto_aof_rewrite_min_size(std::string value)
{
    _options[AUTO_AOF_REWRITE_MIN_SIZE] = value;
}

int Config::get_lua_time_limit()
{
    return _get_int_value(LUA_TIME_LIMIT);
}

void Config::set_lua_time_limit(int value)
{
   _options[LUA_TIME_LIMIT] = boost::lexical_cast<std::string>(value);
}

int Config::get_slowlog_log_slower_than()
{
    return _get_int_value(SLOWLOG_LOG_SLOWER_THAN);
}

void Config::set_slowlog_log_slower_than(int value)
{
    _options[SLOWLOG_LOG_SLOWER_THAN] =
        boost::lexical_cast<std::string>(value);
}

int Config::get_slowlog_max_len()
{
    return _get_int_value(SLOWLOG_MAX_LEN);
}

void Config::set_slowlog_max_len(int value)
{
    _options[SLOWLOG_MAX_LEN] = boost::lexical_cast<std::string>(value);
}

std::string Config::get_notify_keyspace_events()
{
    return _get_string_value(NOTIFY_KEYSPACE_EVENTS);
}

void Config::set_notify_keyspace_events(std::string value)
{
    _options[NOTIFY_KEYSPACE_EVENTS] = value;
}

int Config::get_hash_max_zip_list_entries()
{
    return _get_int_value(HASH_MAX_ZIP_LIST_ENTRIES);
}

void Config::set_hash_max_zip_list_entries(int value)
{
    _options[HASH_MAX_ZIP_LIST_ENTRIES] =
        boost::lexical_cast<std::string>(value);
}

int Config::get_hash_max_zip_list_value()
{
    return _get_int_value(HASH_MAX_ZIP_LIST_VALUE);
}

void Config::set_hash_max_zip_list_value(int value)
{
    _options[HASH_MAX_ZIP_LIST_VALUE] =
        boost::lexical_cast<std::string>(value);
}

int Config::get_list_max_zip_list_entries()
{
    return _get_int_value(LIST_MAX_ZIP_LIST_ENTRIES);
}

void Config::set_list_max_zip_list_entries(int value)
{
    _options[LIST_MAX_ZIP_LIST_ENTRIES] =
        boost::lexical_cast<std::string>(value);
}

int Config::get_list_max_zip_list_value()
{
    return _get_int_value(LIST_MAX_ZIP_LIST_VALUE);
}

void Config::set_list_max_zip_list_value(int value)
{
    _options[LIST_MAX_ZIP_LIST_ENTRIES] =
        boost::lexical_cast<std::string>(value);
}

int Config::get_set_max_intset_entries()
{
    return _get_int_value(SET_MAX_INTSET_ENTRIES);
}

void Config::set_set_max_intset_entries(int value)
{
    _options[SET_MAX_INTSET_ENTRIES] =
        boost::lexical_cast<std::string>(value);
}

int Config::get_zset_max_zip_list_entries()
{
    return _get_int_value(ZSET_MAX_ZIP_LIST_ENTRIES);
}

void Config::set_zset_max_zip_list_entries(int value)
{
    _options[ZSET_MAX_ZIP_LIST_ENTRIES] =
        boost::lexical_cast<std::string>(value);
}

int Config::get_zset_max_zip_list_value()
{
    return _get_int_value(ZSET_MAX_ZIP_LIST_VALUE);
}

void Config::set_zset_max_zip_list_value(int value)
{
    _options[ZSET_MAX_ZIP_LIST_VALUE] =
        boost::lexical_cast<std::string>(value);
}

bool Config::get_active_rehashing()
{
    return _get_bool_value(ACTIVE_REHASHING);
}

void Config::set_active_rehashing(bool value)
{
    if (value)
    {
        _options[ACTIVE_REHASHING] = "yes";
    }
    else
    {
        _options[ACTIVE_REHASHING] = "no";
    }
}

std::vector<std::map <std::string, std::string> >
    Config::get_client_output_buffer_limit()
{
    return _client_output_buffer_limit;
}

void Config::set_client_output_buffer_limit(std::string buff_soft_seconds,
                                            std::string buff_hard_limit,
                                            std::string buff_soft_limit,
                                            std::string buff_class,
                                            bool clear_existing_values)
{
    if (clear_existing_values)
    {
        std::vector<std::map <std::string, std::string> > _override_client_output_buffer_limit;
    }
    std::map<std::string, std::string> buff_limit;
    buff_limit[CLIENT_BUFFER_LIMIT_CLASS] = buff_class;
    buff_limit[CLIENT_BUFFER_LIMIT_HARD_LIMIT] = buff_hard_limit;
    buff_limit[CLIENT_BUFFER_LIMIT_SOFT_LIMIT] = buff_soft_limit;
    buff_limit[CLIENT_BUFFER_LIMIT_SOFT_SECONDS] = buff_soft_seconds;
    _client_output_buffer_limit.push_back(buff_limit);
}

int Config::get_hz()
{
    return _get_int_value(HZ);
}

void Config::set_hz(int value)
{
    _options[HZ] = boost::lexical_cast<std::string>(value);
}

bool Config::get_aof_rewrite_incremental_fsync()
{
    return _get_bool_value(AOF_REWRITE_INCREMENTAL_FSYNC);
}

void Config::set_aof_rewrite_incremental_fsync(bool value)
{
    if (value)
    {
        _options[AOF_REWRITE_INCREMENTAL_FSYNC] = "yes";
    }
    else
    {
        _options[AOF_REWRITE_INCREMENTAL_FSYNC] = "no";
    }
}


}}//end nova::redis
