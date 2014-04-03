#include "pch.hpp"
#include "commands.h"
#include <boost/lexical_cast.hpp>
#include <string>
#include "constants.h"


namespace nova { namespace redis {

//Local constants.

namespace {


static const std::string AUTH_COMMAND = "*2\r\n$4\r\nAUTH\r\n";

static const std::string PING_COMMAND = "*1\r\n$4\r\nPING\r\n";

static const std::string BGSAVE_COMMAND = "*1\r\n$6\r\nBGSAVE\r\n";

static const std::string SAVE_COMMAND  = "*1\r\n$4\r\nSAVE\r\n";

static const std::string LAST_SAVE_COMMAND = "*1\r\n$8\r\nLASTSAVE\r\n";

static const std::string CLIENT_SET_COMMAND = "*3\r\n$6\r\nCLIENT\r\n$7\r\n"
                                                    "SETNAME\r\n";

static const std::string CONFIG_SET_COMMAND = "*4\r\n$6\r\nCONFIG\r\n$3\r\n"
                                                  "SET\r\n";

static const std::string CONFIG_GET_COMMAND = "*3\r\n$6\r\nCONFIG\r\n$3\r\n"
                                                    "GET\r\n";

static const std::string CONFIG_REWRITE_COMMAND = "*2\r\n$6\r\nCONFIG"
                                                        "\r\n$7\r\nREWRITE\r\n";

static const std::string DSIGN  = "$";


}//end anon namespace


void Commands::set_commands()
{
    if(_config_command.length() >= 1)
    {
        std::string formatted_config;
        formatted_config = CRLF + DSIGN +
            boost::lexical_cast<std::string>(_config_command.length()) +
            CRLF + _config_command + CRLF;
        _config_set = "*4" + formatted_config + "$3\r\nSET\r\n";
        _config_get = "*3" + formatted_config + "$3\r\nGET\r\n";
        _config_rewrite = "*2" + formatted_config + "$7\r\nREWRITE\r\n";
    }
    else
    {
        _config_set = CONFIG_SET_COMMAND;
        _config_get = CONFIG_GET_COMMAND;
        _config_rewrite = CONFIG_REWRITE_COMMAND;
    }
}

Commands::Commands(std::string _password, std::string config_command) :
                   _config_set(), _config_get(), _config_rewrite(),
                   _config_command(config_command), password(_password)
{
    set_commands();
}

std::string Commands::auth()
{
    if (password.length() <= 0)
    {
        return "";
    }
    return AUTH_COMMAND + DSIGN +
        boost::lexical_cast<std::string>(password.length()) +
        CRLF + password + CRLF;
}

std::string Commands::ping()
{
    return PING_COMMAND;
}

std::string Commands::config_set(std::string key, std::string value)
{
    key = DSIGN + boost::lexical_cast<std::string>(key.length()) +
          CRLF + key + CRLF;
    value = DSIGN + boost::lexical_cast<std::string>(value.length()) +
            CRLF + value + CRLF;
    return _config_set + key + value;
}

std::string Commands::config_get(std::string key)
{
    key = DSIGN + boost::lexical_cast<std::string>(key.length()) +
          CRLF + key + CRLF;
    return _config_get + key;
}

std::string Commands::config_rewrite()
{
    return _config_rewrite;
}

std::string Commands::bgsave()
{
    return BGSAVE_COMMAND;
}

std::string Commands::save()
{
    return SAVE_COMMAND;
}

std::string Commands::last_save()
{
    return LAST_SAVE_COMMAND;
}

std::string Commands::client_set_name(std::string client_name)
{
    return CLIENT_SET_COMMAND + DSIGN +
           boost::lexical_cast<std::string>(client_name.length()) +
           CRLF + client_name + CRLF;
}


}}//end nova::redis namespace
