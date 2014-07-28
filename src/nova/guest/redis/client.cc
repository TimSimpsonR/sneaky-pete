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

using nova::Log;


namespace nova { namespace redis {


Response Client::_connect()
{
    int retries = 0;
    while (retries <= MAX_RETRIES)
    {
        _socket = get_socket(_host, _port);
        if (_socket >= 0)
        {
            break;
        }
        ++retries;
    }
    if (_socket >= 0)
    {
        return Response(CCONNECTED_RESPONSE, "");
    }
    return Response(CCONNECTION_ERR_RESPONSE, "");
}

Response Client::_send_redis_message(std::string message)
{
    int sres = 0;
    if (_socket < 0)
    {
        Response res = _connect();
        if (res.status != CCONNECTED_RESPONSE)
        {
            return res;
        }
    }
    sres = send_message(_socket, message);
    if (sres == SOCK_ERROR)
    {
        return Response(STIMEOUT_RESPONSE, "");
    }
    return Response(CMESSAGE_SENT_RESPONSE, "");
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
    std::string tmp_byte = "";
    std::string tmp_multi = "";
    std::string tmp_multi_len = "";
    std::string multi_data = "";
    int retries = 0;
    std::string first_byte;
    std::string res = "";
    if (_socket < 0)
    {
        return Response(STIMEOUT_DESCRIPTION, "");
    }
    while (true)
    {
        if (retries == MAX_RETRIES)
        {
            return Response(CTIMEOUT_RESPONSE, "");
        }
        first_byte = get_response(_socket, FIRST_BYTE_READ);
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
                return Response(CTIMEOUT_RESPONSE, "");
            }
            res += get_response(_socket, READ_LEN);
            if (res.length() == 0)
            {
                ++retries;
                continue;
            }
            if (res.substr(res.length() - CRLF_LEN) == CRLF)
            {
                break;
            }
            ++retries;
        }
        if (res.substr(res.length() - CRLF_LEN) != CRLF)
        {
            return Response(SERROR_RESPONSE, "");
        }
        return Response(first_byte,
                            res.substr(0,
                                        res.length() - 2));
    }
    else if (first_byte == MULTIPART_RESPONSE)
    {
        while (true)
        {
            tmp_byte = get_response(_socket, 1);
            if (tmp_byte == "\r")
            {
                get_response(_socket, 2);
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
                    get_response(_socket, 1);
                    tmp_byte = "";
                    multi_len = boost::lexical_cast<int>(tmp_multi_len);
                    break;
                }
                tmp_byte = get_response(_socket, 1);
                tmp_multi_len += tmp_byte;
            }
            multi_data += get_response(_socket,
                                        multi_len);
            if (multi_args != 1)
            {
                multi_data += " ";
                get_response(_socket,
                                3);
            }
            else
            {
                get_response(_socket, 2);
            }
            --multi_args;
        }
        return Response(MULTIPART_RESPONSE,
                                multi_data);
    }
    else
    {
        return Response(UNSUPPORTED_RESPONSE, "");
    }
}

Response Client::_set_client()
{
    if (!_name_set)
    {
        Response res = _send_redis_message(
            _commands->client_set_name(_client_name));
        if (res.status != CMESSAGE_SENT_RESPONSE)
        {
            return res;
        }
        res = _get_redis_response();
        if (res.status == STRING_RESPONSE)
        {
            _name_set = true;
        }
        return res;
    }
    return Response(CNOTHING_TO_DO_RESPONSE, "");
}

Response Client::_auth()
{
    if (_commands->requires_auth() && !_authed)
    {
        Response res = _send_redis_message(_commands->auth());
        if (res.status != CMESSAGE_SENT_RESPONSE)
        {
            return res;
        }
        res = _get_redis_response();
        if (res.status == ERROR_RESPONSE)
        {
            res = _send_redis_message(_commands->auth());
            if (res.status != CMESSAGE_SENT_RESPONSE)
            {
                return res;
            }
            res = _get_redis_response();
            if (res.status == STRING_RESPONSE)
            {
                _authed = true;
            }
        }
        if (res.status == STRING_RESPONSE)
        {
            _authed = true;
        }
        return res;
    }
    _authed = true;
    return Response(CNOTHING_TO_DO_RESPONSE, "");
}

Response Client::_reconnect()
{
    close(_socket);
    _authed = false;
    _name_set = false;
    _socket = -1;
    config = new Config(_config_file);
    _find_config_command();
    _commands = new Commands(config->get_require_pass(),
                             _config_command);
    Response res = _connect();
    if (res.status != CCONNECTED_RESPONSE)
    {
        return res;
    }
    res = _auth();
    if (res.status != STRING_RESPONSE)
    {
        return res;
    }
    res = _set_client();
    if (res.status != STRING_RESPONSE)
    {
        return res;
    }
    return Response(CCONNECTED_RESPONSE, "");
}

void Client::_find_config_command()
{
    for (unsigned int i=0;
            i < config->get_renamed_commands().size();
            i++)
    {
        if (config->get_renamed_commands()[i][RENAMED_COMMAND] ==
            COMMAND_CONFIG)
        {
            _config_command =
                config->get_renamed_commands()[i][NEW_COMMAND_NAME];
        }
    }
}

Client::Client(
    const boost::optional<std::string> & host,
    const boost::optional<std::string> & port,
    const boost::optional<std::string> & client_name,
    const boost::optional<std::string> & config_file)
:   _port(port.get_value_or(REDIS_PORT)),
        _host(host.get_value_or(SOCKET_NAME)),
        _client_name(client_name.get_value_or(REDIS_AGENT_NAME)),
        _config_file(config_file.get_value_or(DEFAULT_REDIS_CONFIG)),
        _config_command(COMMAND_CONFIG)
{
    config = new Config(_config_file);
    _find_config_command();
    _commands = new Commands(config->get_require_pass(),
                             _config_command);
    _authed = false;
    _name_set = false;
    _socket = -1;
    _connect();
    _auth();
    _set_client();
}

Client::~Client()
{
    close(_socket);
}

Response Client::ping()
{
    if (_socket == -1)
    {
        _reconnect();
    }
    Response res = _send_redis_message(_commands->ping());
    if (res.status != CMESSAGE_SENT_RESPONSE)
    {
        return res;
    }
    return _get_redis_response();
}

Response Client::bgsave()
{
    Response res = _send_redis_message(_commands->bgsave());
    if (res.status != CMESSAGE_SENT_RESPONSE)
    {
        return res;
    }
    return _get_redis_response();
}

Response Client::save()
{
    Response res = _send_redis_message(_commands->save());
    if (res.status != CMESSAGE_SENT_RESPONSE)
    {
        return res;
    }
    return _get_redis_response();
}

Response Client::last_save()
{
    Response res = _send_redis_message(
        _commands->last_save());
    if (res.status != CMESSAGE_SENT_RESPONSE)
    {
        return res;
    }
    return _get_redis_response();
}

Response Client::config_get(std::string name)
{
    Response res = _send_redis_message(
        _commands->config_get(name));
    if (res.status != CMESSAGE_SENT_RESPONSE)
    {
        return res;
    }
    return _get_redis_response();
}

Response Client::config_set(std::string name, std::string value)
{
    Response res = _send_redis_message(
        _commands->config_set(name, value));
    if (res.status != CMESSAGE_SENT_RESPONSE)
    {
        return res;
    }
    return _get_redis_response();
}

Response Client::config_rewrite()
{
    Response res = _send_redis_message(
        _commands->config_rewrite());
    if (res.status != CMESSAGE_SENT_RESPONSE)
    {
        return res;
    }
    return _get_redis_response();
}


}}//end nova::redis namespace
