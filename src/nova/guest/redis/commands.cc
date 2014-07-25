#include "pch.hpp"
#include "commands.h"
#include <boost/lexical_cast.hpp>
#include <string>
#include "constants.h"
#include <boost/format.hpp>

using boost::lexical_cast;
using std::string;
using std::stringstream;

namespace nova { namespace redis {

namespace {
    template<typename T>
    void build_redis_array(stringstream & cmd,
                           int & count,
                           const T arg) {
        ++ count;
        const string str_arg = lexical_cast<string>(arg);
        cmd << "$" << str_arg.length() << "\r\n"
            << str_arg << "\r\n";
    }

    template<typename HeadType, typename... TailTypes>
    void build_redis_array(stringstream & cmd,
                           int & count,
                           const HeadType headArg,
                           const TailTypes... tailArgs) {
        build_redis_array(cmd, count, headArg);
        build_redis_array(cmd, count, tailArgs...);
    }

    template<typename... Types>
    string redis_array(const Types... typeArgs) {
        stringstream cmd;
        int count = 0;
        build_redis_array(cmd, count, typeArgs...);
        const string full_cmd = "*" + lexical_cast<string>(count) + "\r\n"
                                + cmd.str();
        return full_cmd;
    }
}

Commands::Commands(const string & _password,
    const boost::optional<string> & config_command)
:   _config_command(config_command.get_value_or("CONFIG")),
    password(_password)
{
}

bool Commands::requires_auth() const {
    return password.length() > 0;
}

string Commands::auth() const {
    if (requires_auth()) {
        return redis_array("AUTH", password);
    } else {
        return "";
    }

}

string Commands::ping() const {
    return redis_array("PING");
}

string Commands::config_set(string key, string value) const {
    return redis_array(_config_command, "SET", key, value);
}

string Commands::config_get(string key) const {
    return redis_array(_config_command, "GET", key);
}

string Commands::config_rewrite() const {
    return redis_array(_config_command, "REWRITE");
}

string Commands::bgsave() const {
    return redis_array("BGSAVE");
}

string Commands::save() const {
    return redis_array("SAVE");
}

string Commands::last_save() const {
    return redis_array("LASTSAVE");
}

string Commands::client_set_name(string client_name) const {
    return redis_array("CLIENT", "SETNAME", client_name);
}


}}//end nova::redis namespace
