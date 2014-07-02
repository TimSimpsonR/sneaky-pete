#include "pch.hpp"
#include "RedisException.h"


namespace nova { namespace redis {

RedisException::RedisException(RedisException::Code code) throw()
: code(code)
{
}

RedisException::~RedisException() throw() {
}

const char * RedisException::what() throw() {
    switch(code) {
        case CHANGE_PASSWORD_ERROR:
            return "Error changing passwords.";
        case COULD_NOT_START:
            return "Could not stop Redis!";
        case COULD_NOT_STOP:
            return "Could not stop Redis!";
        case LOCAL_CONF_READ_ERROR:
            return "Error reading Redis local.conf.";
        case LOCAL_CONF_WRITE_ERROR:
            return "Error writing Redis local conf.";
        case MISSING_ROOT_PASSWORD:
            return "Missing root password argument in prepare call!";
        case PREPARE_ERROR:
            return "An error occured while preparing Redis.";
        case UNABLE_TO_WRITE_CONFIG:
            return "Unable to write new config.";
        default:
            return "An error occurred.";
    }
}

} }  // end namespace
