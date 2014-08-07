#ifndef COMMANDS_H
#define COMMANDS_H

#include <boost/lexical_cast.hpp>
#include <boost/optional.hpp>
#include <string>


namespace nova { namespace redis {


class Commands
{
    public:
        Commands(const std::string & _password);

        std::string auth() const;

        std::string ping() const;

        std::string config_set(std::string key, std::string value) const;

        std::string config_get(std::string key) const;

        std::string config_rewrite() const;

        std::string info() const;

        std::string bgsave() const;

        std::string save() const;

        std::string last_save() const;

        std::string client_set_name(std::string client_name) const;

        bool requires_auth() const;

    private:

        const std::string password;
};


}}//end nova::redis namespace
#endif /* COMMANDS_H */
