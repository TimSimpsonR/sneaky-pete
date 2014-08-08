#ifndef __NOVA_REDIS_REDISEXCEPTION_H
#define __NOVA_REDIS_REDISEXCEPTION_H

#include <exception>

namespace nova { namespace redis {
class RedisException : public std::exception {

    public:
        enum Code {
            CANT_SET_NAME,
            CHANGE_PASSWORD_ERROR,
            CONNECTION_ERROR,
            COULD_NOT_AUTH,
            COULD_NOT_START,
            COULD_NOT_STOP,
            LOCAL_CONF_READ_ERROR,
            LOCAL_CONF_WRITE_ERROR,
            MISSING_ROOT_PASSWORD,
            PREPARE_ERROR,
            RESPONSE_TIMEOUT,
            UNABLE_TO_WRITE_CONFIG
        };

        RedisException(Code code) throw();

        virtual ~RedisException() throw();

        virtual const char * what() const throw();

        const Code code;
};

} }

#endif
