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
 *- FlagValues
 *---------------------------------------------------------------------------*/

void FlagValues::add_from_arg(const char * arg, bool ignore_mismatch) {
    string line = arg;
    add_from_arg(line, ignore_mismatch);
}

// Adds a line in the form "--name=value"
void FlagValues::add_from_arg(const std::string & arg, bool ignore_mismatch) {
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
    } else {
        throw FlagException(FlagException::DUPLICATE_FLAG_VALUE, line.c_str());
    }
}

// Opens up a file and adds everything.
void FlagValues::add_from_file(const char * file_path) {
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

FlagValuesPtr FlagValues::create_from_args(size_t count, char** argv,
                                           bool ignore_mismatch) {
    FlagValuesPtr flags(new FlagValues());
    for (size_t i = 0; i < count; i ++) {
        flags->add_from_arg(argv[i], ignore_mismatch);
    }
    return flags;
}

FlagValuesPtr FlagValues::create_from_file(const char* file_path) {
    FlagValuesPtr flags(new FlagValues());
    flags->add_from_file(file_path);
    return flags;
}

const char * FlagValues::get(const char * const name) {
    if (map.count(name) == 0) {
        throw FlagException(FlagException::KEY_NOT_FOUND, name);
    }
    return map[name].c_str();
}

int FlagValues::get_as_int(const char * const name) {
    if (map.count(name) == 0) {
        throw FlagException(FlagException::KEY_NOT_FOUND, name);
    }
    return boost::lexical_cast<int>(map[name].c_str());
}

void FlagValues::get_sql_connection(string & host, string & user,
                                    string & password,
                                    optional<string> & database) {
    const char * value = get("sql_connection");
    //--sql_connection=mysql://nova:novapass@10.0.4.15/nova
    Regex regex("mysql:\\/\\/(\\w+):(\\w+)@([0-9\\.]+)\\/(\\w+)");
    RegexMatchesPtr matches = regex.match(value);
    if (!matches) {
        throw FlagException(FlagException::PATTERN_NOT_MATCHED, value);
    }
    for (int i = 0; i < 3; i ++) {
        if (!matches->exists_at(i)) {
            throw FlagException(FlagException::PATTERN_GROUP_NOT_MATCHED,
                                value);
        }
    }
    user = matches->get(1);
    password = matches->get(2);
    host = matches->get(3);
    if (!matches->exists_at(4)) {
        database = boost::none;
    } else {
        database = optional<string>(matches->get(4));
    }
}

} } // end nova::flags
