#include "pch.hpp"
#include "RedisClient.h"
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/assert.hpp>
#include <hiredis/hiredis.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include "nova/Log.h"
#include "nova/guest/redis/config.h"
#include "nova/redis/RedisException.h"

using std::string;
using std::stringstream;
using boost::optional;

namespace nova { namespace redis {


/*****************************************************************************
 * RedisClient::Info
 *****************************************************************************/

RedisClient::Info::Info(const std::string & info)
:   info_stream(info) {
}

RedisClient::Info::Info(const RedisClient::Info & other)
:   info_stream(other.info_stream.str()) {
}

string RedisClient::Info::get(const std::string & key) {
    const auto value = get_optional(key);
    if (value) {
        return value.get();
    }
    NOVA_LOG_ERROR("Couldn't find key %s in Redis's info.", key);
    throw RedisException(RedisException::UNKNOWN_INFO_VALUE);
}

optional<string> RedisClient::Info::get_optional(const std::string & key) {
    info_stream.seekg(0, std::ios_base::beg);
    const string find = key + ":";
    string line;
    while(std::getline(info_stream, line)) {
        if (boost::starts_with(line, find)) {
            line = line.substr(find.length());
            boost::trim(line);
            return line;
        }
    }
    return boost::none;
}

/*****************************************************************************
 * RedisClient::Reply
 *****************************************************************************/

RedisClient::Reply::Reply(redisReply * r)
:   r(r) {
}

RedisClient::Reply::~Reply() {
    freeReplyObject(r);
}

string RedisClient::Reply::expect_status() const {
    throw_if_error();
    if (REDIS_REPLY_STATUS == r->type) {
        return r->str;
    } else {
        NOVA_LOG_ERROR("Redis reply was not a status. Type = %d",
                       r->type);
        throw RedisException(RedisException::UNEXPECTED_RESPONSE);
    }
}

string RedisClient::Reply::expect_string() const  {
    throw_if_error();
    if (REDIS_REPLY_STRING == r->type) {
        return r->str;
    } else {
        NOVA_LOG_ERROR("Redis reply was not a string. Type = %d",
                       r->type);
        throw RedisException(RedisException::UNEXPECTED_RESPONSE);
    }
}

void RedisClient::Reply::expect_ok() const {
    string result = expect_status();
    if (result != "OK") {
        NOVA_LOG_ERROR("Did not get OK from Redis! Result == %s", result);
        throw RedisException(RedisException::UNEXPECTED_RESPONSE);
    }
}

long long RedisClient::Reply::expect_int() const  {
    throw_if_error();
    if (REDIS_REPLY_INTEGER == r->type) {
        return r->integer;
    } else {
        NOVA_LOG_ERROR("Redis reply was an integer.  Type = %d",
                       r->type);
        throw RedisException(RedisException::UNEXPECTED_RESPONSE);
    }
}

redisReply * RedisClient::Reply::get() {
    return r;
}

void RedisClient::Reply::throw_if_error() const {
    if (REDIS_REPLY_ERROR == r->type) {
        NOVA_LOG_ERROR("Redis replied with ERROR:%s", r->str);
        throw RedisException(RedisException::REPLY_ERROR);
    }
}



namespace {

    const timeval default_timeout = { 1, 500000 };

}  // end anon namespace


/*****************************************************************************
 * RedisClient
 *****************************************************************************/

RedisClient::RedisClient()
:   context(redisConnectWithTimeout("localhost", 6379, default_timeout)) {
    if (NULL == context) {
        NOVA_LOG_ERROR("Error allocating Redis client memory.");
        throw RedisException(RedisException::CONNECTION_ERROR);
    } else if (context->err) {
        NOVA_LOG_ERROR("Redis connection error: %s\n", context->errstr);
        throw RedisException(RedisException::CONNECTION_ERROR);
    }
    BOOST_ASSERT((context->flags | REDIS_BLOCK) != REDIS_BLOCK);
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

RedisClient::Reply RedisClient::command(const char * format, ...) {
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

optional<string> RedisClient::get_role() {
    stringstream info;
    info << command("INFO").expect_string();
    string line;
    while(std::getline(info, line)) {
        if (boost::starts_with(line, "role:")) {
            line = line.substr(5);
            return line;
        }
    }
    return boost::none;
}

RedisClient::Info RedisClient::info() {
    Info i(command("INFO").expect_string());
    return i;
}

void RedisClient::ping() {
    string res = command("PING").expect_status();
    NOVA_LOG_INFO("PING? %s", res);
}




} }
