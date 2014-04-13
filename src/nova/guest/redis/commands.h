#ifndef COMMANDS_H
#define COMMANDS_H

#include <string>


namespace nova { namespace redis {


class Commands
{
    private:
        std::string _config_set;

        std::string _config_get;

        std::string _config_rewrite;

        std::string _config_command;

        void set_commands();

    public:
        std::string password;

        Commands(std::string _password, std::string config_command);

        std::string auth();

        std::string ping();

        std::string config_set(std::string key, std::string value);

        std::string config_get(std::string key);

        std::string config_rewrite();

        std::string bgsave();

        std::string save();

        std::string last_save();

        std::string client_set_name(std::string client_name);

};


}}//end nova::redis namespace
#endif /* COMMANDS_H */
