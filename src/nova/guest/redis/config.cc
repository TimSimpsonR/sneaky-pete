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
#include "nova/Log.h"
#include <boost/optional.hpp>
#include "constants.h"

using nova::Log;
using boost::optional;
using std::string;


namespace nova { namespace redis {

namespace {

} // end anon namespace

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

Config::Config(const optional<string> & config)
:   _redis_config(config.get_value_or("/etc/redis/redis.conf"))
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
        //TODO(tim.simpson): None of these private variables are ever accessed.
        //                   Delete?
        if(value == BIND_ADDR)
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
            _options[value] = option_value;
        }
    }
    rconfig.close();

}

std::string Config::get_log_file()
{
    return _get_string_value(LOG_FILE);
}

std::string Config::get_pidfile()
{
    return _get_string_value(PIDFILE);
}

int Config::get_port()
{
    return _get_int_value(PORT);
}

std::string Config::get_db_filename()
{
    return _get_string_value(DB_FILENAME);
}

std::string Config::get_db_dir()
{
    return _get_string_value(DB_DIR);
}

std::string Config::get_max_memory()
{
    return _get_string_value(MAX_MEMORY);
}

std::string Config::get_require_pass()
{
    return _get_string_value(REQUIRE_PASS);
}

std::string Config::get_append_filename()
{
    return _get_string_value(APPEND_FILENAME);
}

}}//end nova::redis
