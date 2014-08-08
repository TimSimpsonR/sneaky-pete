#include "pch.hpp"
#include "RedisClient.h"
#include <boost/assert.hpp>
#include <hiredis/hiredis.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include "nova/Log.h"
#include "nova/guest/redis/config.h"
#include "nova/redis/RedisException.h"

using std::string;


namespace nova { namespace redis {

/* Manages a reply object to ensure it's always freed. */
class ReplyPtr {
    public:

        ReplyPtr(redisReply * r)
        :   r(r)    {
        }

        string expect_status() const {
            throw_if_error();
            if (REDIS_REPLY_STATUS == r->type) {
                return r->str;
            } else {
                NOVA_LOG_ERROR("Redis reply was not a status. Type = %d",
                               r->type);
                throw RedisException(RedisException::UNEXPECTED_RESPONSE);
            }
        }

        string expect_string() const  {
            throw_if_error();
            if (REDIS_REPLY_STRING == r->type) {
                return r->str;
            } else {
                NOVA_LOG_ERROR("Redis reply was not a string. Type = %d",
                               r->type);
                throw RedisException(RedisException::UNEXPECTED_RESPONSE);
            }
        }

        void expect_ok() const {
            string result = expect_status();
            if (result != "OK") {
                NOVA_LOG_ERROR("Did not get OK from Redis!");
                throw RedisException(RedisException::UNEXPECTED_RESPONSE);
            }
        }

        long long expect_int() const  {
            throw_if_error();
            if (REDIS_REPLY_INTEGER == r->type) {
                return r->integer;
            } else {
                NOVA_LOG_ERROR("Redis reply was an integer.  Type = %d",
                               r->type);
                throw RedisException(RedisException::UNEXPECTED_RESPONSE);
            }
        }

        redisReply * get() {
            return r;
        }

        void throw_if_error() const {
            if (REDIS_REPLY_ERROR == r->type) {
                NOVA_LOG_ERROR("Redis replied with ERROR:%s", r->str);
                throw RedisException(RedisException::REPLY_ERROR);
            }
        }

        ~ReplyPtr() {
            freeReplyObject(r);
        }

    private:
        redisReply * r;
};

namespace {

    const timeval default_timeout = { 1, 500000 };

}  // end anon namespace


RedisClient::RedisClient()
:   context(redisConnectWithTimeout("localhost", 6379, default_timeout)) {
    if (NULL == context) {
        NOVA_LOG_ERROR("Error allocating Redis client memory.");
        throw RedisException(RedisException::CONNECTION_ERROR);
    } else if (context->err) {
        NOVA_LOG_ERROR("Redis connection error: %s\n", context->errstr);
        throw RedisException(RedisException::CONNECTION_ERROR);
    }
    BOOST_ASSERT_MSG(((context.flags | REDIS_BLOCK) != REDIS_BLOCK),
                     "Redis not set to blocking.");
    Config config;
    string password = config.get_require_pass();
    if (password.length() > 0) {
        command("AUTH %s", password.c_str())
            .expect_ok();
    }
    command("CLIENT SETNAME trove-guestagent")
        .expect_ok();
}

RedisClient::~RedisClient() {
     redisFree(context);
}

ReplyPtr RedisClient::command(const char * format, ...) {
    va_list vargs;  // Careful: don't throw exceptions between here and va_end.
    va_start(vargs, format);
    void * result = redisvCommand(context, format, vargs);
    va_end(vargs);

    if (NULL == result) {
        NOVA_LOG_ERROR("Error running redisCommand: %s", format);
        if (context->err) {
            NOVA_LOG_ERROR("redisContext error: %s", context->errstr);
        }
        throw RedisException(RedisException::COMMAND_ERROR);
    }
    //Note: I'm not sure why hiredis returns void, but this
    //      appears to be safe.
    return static_cast<redisReply *>(result);
}

void RedisClient::config_rewrite() {
    command("CONFIG", "REWRITE")
        .expect_ok();
}

void RedisClient::config_set(const string & key, const string & value) {
    command("CONFIG SET %s %s", key.c_str(), value.c_str())
        .expect_ok();
}

void RedisClient::ping() {
    string res = command("PING").expect_status();
    NOVA_LOG_INFO("PING? %s", res);
}



} }
