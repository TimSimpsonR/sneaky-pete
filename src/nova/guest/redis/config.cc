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

namespace {

std::string remove_quotes(const std::string & value) {
    if(value.length() > 0 &&
       value.at(0) == '"' &&
       value.at(value.length() - 1) == '"') {
        return value.substr(1, value.length() - 2);
    }
    return value;
}
}//end anon namespace



namespace nova { namespace redis {

namespace {

} // end anon namespace

std::string Config::_get_string_value(std::string key) {
    if (_options.count(key) > 0) {
        return _options[key];
    }
    return "";
}

int Config::_get_int_value(std::string key) {
    if (_options.count(key) > 0) {
        return boost::lexical_cast<int>(_options[key]);
    }
    return INT_NULL;
}

Config::Config(const optional<string> & config)
:   _redis_config(config.get_value_or("/etc/redis/redis.conf")) {
    std::string line;
    std::string value;
    std::ifstream rconfig(_redis_config.c_str());
    if (!rconfig.is_open()) {
        return;
    }
    while(std::getline(rconfig, line)) {
        if (line.length() <= 1) {
            continue;
        }
        std::istringstream iss(line);
        iss >> value;
        value = remove_quotes(value);
        if(value == SLAVE_OF) {
            iss >> value;
            _slave_of[MASTER_IP] = remove_quotes(value);
            iss >> value;
            _slave_of[MASTER_PORT] = remove_quotes(value);
        }
        else {
            std::string option_value;
            iss >> option_value;
            _options[value] = remove_quotes(option_value);
        }
    }
    rconfig.close();

}

int Config::get_port() {
    return _get_int_value(PORT);
}

std::string Config::get_pidfile() {
    return _get_string_value(PIDFILE);
}

std::string Config::get_db_filename() {
    return _get_string_value(DB_FILENAME);
}

std::string Config::get_db_dir() {
    return _get_string_value(DB_DIR);
}

std::map<std::string, std::string> Config::get_slave_of() {
    return _slave_of;
}

std::string Config::get_master_auth() {
    return _get_string_value(MASTER_AUTH);
}

std::string Config::get_require_pass() {
    return _get_string_value(REQUIRE_PASS);
}

std::string Config::get_max_memory() {
    return _get_string_value(MAX_MEMORY);
}

std::string Config::get_append_filename() {
    return _get_string_value(APPEND_FILENAME);
}

std::string Config::get_log_file() {
    return _get_string_value(LOG_FILE);
}

}}//end nova::redis
