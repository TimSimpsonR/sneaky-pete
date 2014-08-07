#include "pch.hpp"
#include "client.h"
#include <string>
#include <iostream>
#include <boost/lexical_cast.hpp>
#include "connection.h"
#include "constants.h"
#include "response.h"
#include "commands.h"
#include "config.h"
#include "nova/Log.h"
#include "nova/redis/RedisException.h"

using nova::Log;
using std::string;


namespace nova { namespace redis {


namespace {

    //Max number of retries.
    static const int MAX_RETRIES = 100;
}


Response Client::_send_redis_message(const string & message) {
    //TODO(tim.simpson): Maybe change it to look for the good return value
    //                   rather than avoid the one bad one.
    NOVA_LOG_TRACE("Sending redis message: %s", message);
    if (SOCK_ERROR == _socket.send_message(message)) {
        throw RedisException(RedisException::RESPONSE_TIMEOUT);
    }
    auto res = _get_redis_response();
    NOVA_LOG_TRACE("Redis response: %s", res.status);
    return res;
}

Response Client::_get_redis_response()
{
    /*
    * Determine the response type from the redis server and return
    * A struct containing the response type as a string,
    * a string of its data and a description of the response type.
    * This method accepts an integer to an active socket file
    * descriptor.
    */
    int multi_args = 0;
    int multi_len = 0;
    string tmp_byte = "";
    string tmp_multi = "";
    string tmp_multi_len = "";
    string multi_data = "";
    int retries = 0;
    string first_byte;
    string res = "";
    while (true)
    {
        if (retries == MAX_RETRIES)
        {
            throw RedisException(RedisException::RESPONSE_TIMEOUT);
        }
        first_byte = _socket.get_response(FIRST_BYTE_READ);
        if (first_byte.length() > 0)
        {
            break;
        }
        ++retries;
    }
    retries = 0;
    if (first_byte == STRING_RESPONSE ||
        first_byte == ERROR_RESPONSE ||
        first_byte == INTEGER_RESPONSE)
    {
        while (true)
        {
            if (retries == MAX_RETRIES)
            {
                throw RedisException(RedisException::RESPONSE_TIMEOUT);
            }
            res += _socket.get_response(READ_LEN);
            if (res.length() == 0)
            {
                ++retries;
                continue;
            }
            if (res.substr(res.length() - CRLF.length()) == CRLF)
            {
                break;
            }
            ++retries;
        }
        if (res.substr(res.length() - CRLF.length()) != CRLF)
        {
            return Response(SERROR_RESPONSE);
        }
        return Response(first_byte);
    }
    else if (first_byte == MULTIPART_RESPONSE)
    {
        while (true)
        {
            tmp_byte = _socket.get_response(1);
            if (tmp_byte == "\r")
            {
                _socket.get_response(2);
                multi_args = boost::lexical_cast<int>(tmp_multi);
                tmp_byte = "";
                break;
            }
            tmp_multi += tmp_byte;
        }
        while (multi_args != 0)
        {
            tmp_multi_len = "";
            while (true)
            {
                if (tmp_byte == "\r")
                {
                    _socket.get_response(1);
                    tmp_byte = "";
                    multi_len = boost::lexical_cast<int>(tmp_multi_len);
                    break;
                }
                tmp_byte = _socket.get_response(1);
                tmp_multi_len += tmp_byte;
            }
            multi_data += _socket.get_response(multi_len);
            if (multi_args != 1)
            {
                multi_data += " ";
                _socket.get_response(3);
            }
            else
            {
                _socket.get_response(2);
            }
            --multi_args;
        }
        return Response(MULTIPART_RESPONSE);
    }
    else
    {
        return Response(UNSUPPORTED_RESPONSE);
    }
}

void Client::_set_client()
{
    Response res = _send_redis_message(
        _commands->client_set_name("trove-guestagent"));
    if (res.status != STRING_RESPONSE)
    {
        NOVA_LOG_ERROR("Failure calling CLIENT SETNAME on Redis!");
        throw RedisException(RedisException::CANT_SET_NAME);
    }
}

bool Client::_auth_required() {
    return _commands->requires_auth();
}

void Client::_auth()
{
    if (_auth_required())
    {
        NOVA_LOG_TRACE("Redis auth required, sending auth.");
        const Response res = _send_redis_message(_commands->auth());
        if (res.status == STRING_RESPONSE) {
            NOVA_LOG_TRACE("Authed!");
        } else {
            NOVA_LOG_ERROR("Couldn't auth to Redis.");
            throw RedisException(RedisException::COULD_NOT_AUTH);
        }
    }
}

Client::Client(
    const boost::optional<string> & host,
    const boost::optional<int> & port,
    const boost::optional<string> & password)
:   _commands(),
    _socket(host.get_value_or("localhost"), port.get_value_or(6379),
            MAX_RETRIES)
{
    string pw;
    if (password) {
        pw = password.get();
    } else {
        Config config;
        pw = config.get_require_pass();
    }
    _commands.reset(new Commands(pw));
    _auth();
    _set_client();
}

Client::~Client()
{
}

Response Client::ping()
{
    return _send_redis_message(_commands->ping());
}

Response Client::info() {
    return _send_redis_message(_commands->info());
}

Response Client::bgsave()
{
    return _send_redis_message(_commands->bgsave());
}

Response Client::save()
{
    return _send_redis_message(_commands->save());
}

Response Client::last_save()
{
    return _send_redis_message(_commands->last_save());
}

Response Client::config_get(string name)
{
    return _send_redis_message(_commands->config_get(name));
}

Response Client::config_set(string name, string value)
{
    return _send_redis_message(_commands->config_set(name, value));
}

Response Client::config_rewrite()
{
    return _send_redis_message(_commands->config_rewrite());
}


}}//end nova::redis namespace
