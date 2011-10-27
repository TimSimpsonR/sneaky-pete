#include "nova/flags.h"
#include <boost/format.hpp>
#include <boost/lexical_cast.hpp>
#include <fstream>
#include <string.h>


using boost::format;
using std::string;

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

void FlagValues::add_from_arg(const char * arg) {
    string line = arg;
    add_from_arg(line);
}

// Adds a line in the form "--name=value"
void FlagValues::add_from_arg(const std::string & arg) {
    string line = arg;
    if (line.size() < 2 || line.substr(0, 2) != "--") {
        throw FlagException(FlagException::NO_STARTING_DASHES, line.c_str());
    }
    size_t index = line.find("=", 2);
    if (index == string::npos) {
        throw FlagException(FlagException::NO_EQUAL_SIGN, line.c_str());
    }
    string name = line.substr(2, index - 2);
    string value = line.substr(index + 1, line.size() - 1);
    if (map.count(name) == 0) {
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
    }
    file.close();
}

FlagValuesPtr FlagValues::create_from_args(size_t count, const char* argv[]) {
    FlagValuesPtr flags(new FlagValues());
    for (size_t i = 0; i < count; i ++) {
        flags->add_from_arg(argv[i]);
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

const char * FlagValues::get_as_int(const char * const name) {
    if (map.count(name) == 0) {
        throw FlagException(FlagException::KEY_NOT_FOUND, name);
    }
    return boost:lexical_cast<int>(map[name].c_str());
}


} } // end nova::flags
