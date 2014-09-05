#ifndef __NOVA_REDIS_REDISEXCEPTION_H
#define __NOVA_REDIS_REDISEXCEPTION_H

#include <exception>

namespace nova { namespace redis {
class RedisException : public std::exception {

    public:
        enum Code {
            BACKUP_FILE_MISSING,
            CANT_SET_NAME,
            CHANGE_PASSWORD_ERROR,
            COMMAND_ERROR,
            CONNECTION_ERROR,
            COULD_NOT_AUTH,
            COULD_NOT_FSYNC,
            COULD_NOT_START,
            COULD_NOT_STOP,
            INVALID_BACKUP_TARGET,
            NO_BACKUP_STRATEGY_IN_USE,
            UNEXPECTED_RESPONSE,
            LOCAL_CONF_READ_ERROR,
            LOCAL_CONF_WRITE_ERROR,
            MISSING_ROOT_PASSWORD,
            PREPARE_ERROR,
            REPLY_ERROR,
            RESPONSE_TIMEOUT,
            PROCESS_FAILED,
            UNABLE_TO_WRITE_CONFIG,
            UNKNOWN_INFO_VALUE
        };

        RedisException(Code code) throw();

        virtual ~RedisException() throw();

        virtual const char * what() const throw();

        const Code code;
};

} }

#endif
