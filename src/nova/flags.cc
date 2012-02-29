#include "nova/flags.h"
#include <boost/format.hpp>
#include <boost/lexical_cast.hpp>
#include "nova/utils/regex.h"
#include <fstream>
#include <string.h>


using boost::format;
using boost::optional;
using std::string;
using nova::utils::Regex;
using nova::utils::RegexMatchesPtr;

namespace nova { namespace flags {

/**---------------------------------------------------------------------------
 *- FlagException
 *---------------------------------------------------------------------------*/

FlagException::FlagException(Code code, const char * line) throw()
: code(code), use_msg(false) {
    try {
        string msg = str(format("%s:%s") % code_to_string(code) % line);
        strncpy(this->msg, msg.c_str(), 256);
        use_msg = true;
    } catch(...) {
        // Maybe this is paranoid, but just in case...
        use_msg = false;
    }
}

FlagException::~FlagException() throw() {
}

const char * FlagException::code_to_string(FlagException::Code code) throw() {
    switch(code) {
        case DUPLICATE_FLAG_VALUE:
            return "An attempt was made to add two flags with the same value.";
        case FILE_NOT_FOUND:
            return "File not found.";
        case INVALID_FORMAT:
            return "Invalid format for flag file value.";
        case KEY_NOT_FOUND:
            return "A flag value with the given key was not found.";
        case NO_EQUAL_SIGN:
            return "No equal sign was found in the given line.";
        case NO_STARTING_DASHES:
            return "No starting dashes were found on the line of input.";
        case PATTERN_GROUP_NOT_MATCHED:
            return "A regex pattern group not matched.";
        case PATTERN_NOT_MATCHED:
            return "The regex pattern was not matched.";
        default:
            return "An error occurred.";
    }
}

const char * FlagException::what() const throw() {
    if (use_msg) {
        return msg;
    } else {
        return code_to_string(code);
    }
}

/**---------------------------------------------------------------------------
 *- FlagMap
 *---------------------------------------------------------------------------*/

void FlagMap::add_from_arg(const char * arg, bool ignore_mismatch) {
    string line = arg;
    add_from_arg(line, ignore_mismatch);
}

// Adds a line in the form "--name=value"
void FlagMap::add_from_arg(const std::string & arg, bool ignore_mismatch) {
    string line = arg;
    if (line.size() > 0 && line.substr(0, 1) == "#") {
        return;
    }
    if (line.size() < 2 || line.substr(0, 2) != "--") {
        if (ignore_mismatch) {
            return;
        } else {
            throw FlagException(FlagException::NO_STARTING_DASHES, line.c_str());
        }
    }
    size_t index = line.find("=", 2);
    if (index == string::npos) {
        if (ignore_mismatch) {
            return;
        } else {
            throw FlagException(FlagException::NO_EQUAL_SIGN, line.c_str());
        }
    }
    string name = line.substr(2, index - 2);
    string value = line.substr(index + 1, line.size() - 1);
    if (map.count(name) == 0) {
        if (name == "flagfile") {
            add_from_file(value.c_str());
        }
        map[name] = value;
    } else if (map[name] != value) {
        throw FlagException(FlagException::DUPLICATE_FLAG_VALUE, line.c_str());
    }
}

// Opens up a file and adds everything.
void FlagMap::add_from_file(const char * file_path) {
    std::ifstream file(file_path);
    if (!file.is_open()) {
        throw FlagException(FlagException::FILE_NOT_FOUND, file_path);
    }
    string line;
    while(file.good()) {
        getline(file, line);
        add_from_arg(line, true);
    }
    file.close();
}

FlagMapPtr FlagMap::create_from_args(size_t count, char** argv,
                                           bool ignore_mismatch) {
    FlagMapPtr flags(new FlagMap());
    for (size_t i = 0; i < count; i ++) {
        flags->add_from_arg(argv[i], ignore_mismatch);
    }
    return flags;
}

FlagMapPtr FlagMap::create_from_file(const char* file_path) {
    FlagMapPtr flags(new FlagMap());
    flags->add_from_file(file_path);
    return flags;
}

const char * FlagMap::get(const char * const name) {
    return get(name, true);
}

const char * FlagMap::get(const char * const name,
                             const char * const default_value) {
    const char * value = get(name, false);
    if (value == 0) {
        return default_value;
    }
    return value;
}

const char * FlagMap::get(const char * const name, bool throw_on_missing) {
    if (map.find(name) == map.end()) {
        if (throw_on_missing) {
            throw FlagException(FlagException::KEY_NOT_FOUND, name);
        } else {
            return 0;
        }
    }
    return map[name].c_str();
}

int FlagMap::get_as_int(const char * const name) {
    if (map.count(name) == 0) {
        throw FlagException(FlagException::KEY_NOT_FOUND, name);
    }
    return boost::lexical_cast<int>(map[name].c_str());
}

int FlagMap::get_as_int(const char * const name, int default_value) {
    if (map.count(name) == 0) {
        map[name] = str(format("%d") % default_value);
    }
    return boost::lexical_cast<int>(map[name].c_str());
}

void FlagMap::get_sql_connection(string & host, string & user,
                                 string & password,
                                 string & database) {
    const char * value = get("sql_connection");
    //--sql_connection=mysql://nova:novapass@10.0.4.15/nova
    Regex regex("mysql:\\/\\/(\\w+):(\\w+)@([0-9\\.]+)\\/(\\w+)");
    RegexMatchesPtr matches = regex.match(value);
    if (!matches) {
        throw FlagException(FlagException::PATTERN_NOT_MATCHED, value);
    }
    for (int i = 0; i < 4; i ++) {
        if (!matches->exists_at(i)) {
            throw FlagException(FlagException::PATTERN_GROUP_NOT_MATCHED,
                                value);
        }
    }
    user = matches->get(1);
    password = matches->get(2);
    host = matches->get(3);
    database = matches->get(4);
}

template<typename T>
optional<T> get_flag_value(FlagMap & map, const char * name) {
    const char * value = map.get(name, false);
    if (value == 0) {
        return boost::none;
    }
    try {
        return optional<T>( boost::lexical_cast<T>(value) );
    } catch(const boost::bad_lexical_cast & blc) {
        throw FlagException(FlagException::INVALID_FORMAT, value);
    }
}

template<>
optional<const char *> get_flag_value<>(FlagMap & map, const char * name) {
    const char * value = map.get(name, false);
    if (value == 0) {
        return boost::none;
    }
    return optional<const char *>(value);
}

template<>
optional<bool> get_flag_value<>(FlagMap & map, const char * name) {
    optional<const char *> value = get_flag_value<const char *>(map, name);
    if (value) {
        return optional<bool>(strncmp(value.get(), "true", 4) == 0);
    } else {
        return boost::none;
    }
}


template<typename T>
T get_flag_value(FlagMap & map, const char * name, T default_value) {
    return get_flag_value<T>(map, name).get_value_or(default_value);
}

/**---------------------------------------------------------------------------
 *- FlagValues
 *---------------------------------------------------------------------------*/

FlagValues::FlagValues(FlagMapPtr map)
: map(map)
{
    map->get_sql_connection(_nova_sql_host, _nova_sql_user,
                            _nova_sql_password, _nova_sql_database);
}

const char * FlagValues::apt_self_package_name() const {
    return map->get("apt_self_package_name", "nova-guest");
}

int FlagValues::apt_self_update_time_out() const {
    return get_flag_value<int>(*map, "apt_self_update_time_out", 1 * 60);
}

bool FlagValues::apt_use_sudo() const {
    const char * value = map->get("apt_use_sudo", "true");
    return strncmp(value, "true", 4) == 0;
}

const char * FlagValues::control_exchange() const {
    return map->get("control_exchange", "nova");
}

const char * FlagValues::db_backend() const {
    return map->get("db_backend", "sqlalchemy");
}

const char * FlagValues::guest_ethernet_device() const {
    return map->get("guest_ethernet_device", "eth0");
}

optional<const char *> FlagValues::host() const {
    return get_flag_value<const char *>(*map, "host");
}

optional<int> FlagValues::log_file_max_old_files() const {
    return get_flag_value<int>(*map, "log_file_max_old_files");
}

optional<size_t> FlagValues::log_file_max_size() const {
    return get_flag_value<size_t>(*map, "log_file_max_size");
}

boost::optional<double> FlagValues::log_file_max_time() const {
    return get_flag_value<double>(*map, "log_file_max_time");
}

optional<const char *> FlagValues::log_file_path() const {
    return get_flag_value<const char *>(*map, "log_file_path");
}

bool FlagValues::log_use_std_streams() const {
    return get_flag_value<bool>(*map, "log_use_std_streams", true);
}

int FlagValues::mysql_state_change_wait_time() const {
    return get_flag_value<int>(*map, "mysql_state_change_wait_time", 2 * 60);
}

const char * FlagValues::node_availability_zone() const {
    return map->get("node_availability_zone", "nova");
}


unsigned long FlagValues::nova_db_reconnect_wait_time() const {
    return get_flag_value(*map, "nova_db_reconnect_wait_time",
                          (unsigned long) 30);
}

const char * FlagValues::nova_sql_database() const {
    return _nova_sql_database.c_str();
}

const char * FlagValues::nova_sql_host() const {
    return _nova_sql_host.c_str();
}

const char * FlagValues::nova_sql_password() const {
    return _nova_sql_password.c_str();
}

unsigned long FlagValues::nova_sql_reconnect_wait_time() const {
    return get_flag_value(*map, "nova_sql_reconnect_wait_time",
                          (unsigned long) 30);
}

const char * FlagValues::nova_sql_user() const {
    return _nova_sql_user.c_str();
}

unsigned long FlagValues::periodic_interval() const {
    return get_flag_value(*map, "periodic_interval", (unsigned long) 60);
}

optional<int> FlagValues::preset_instance_id() const {
    return get_flag_value<int>(*map, "preset_instance_id");
}

size_t FlagValues::rabbit_client_memory() const {
    return get_flag_value(*map, "rabbit_client_memory", (size_t) 4096);
}

const char * FlagValues::rabbit_host() const {
    return map->get("rabbit_host", "localhost");
}

const char * FlagValues::rabbit_password() const {
    return map->get("rabbit_password", "guest");
}

const int FlagValues::rabbit_port() const {
    return map->get_as_int("rabbit_port", 5672);
}

unsigned long FlagValues::rabbit_reconnect_wait_time() const {
    return get_flag_value(*map, "rabbit_reconnect_wait_time",
                                 (unsigned long) 30);
}

const char * FlagValues::rabbit_userid() const {
    return map->get("rabbit_userid", "guest");
}

unsigned long FlagValues::report_interval() const {
    return get_flag_value(*map, "report_interval", (unsigned long) 10);
}

bool FlagValues::use_syslog() const {
    return get_flag_value<bool>(*map, "use_syslog", false);
}

} } // end nova::flags
