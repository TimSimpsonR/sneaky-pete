#ifndef CONFIG_H
#define CONFIG_H

#include <map>
#include <vector>
#include <string>


namespace nova { namespace redis {


class Config
{
    private:
        std::map<std::string, std::string>_options;

        std::vector<std::string> _bind_addrs;

        std::vector<std::map <std::string, std::string> > _save;

        std::map<std::string, std::string> _slave_of;

        std::vector<std::map <std::string, std::string> > _renamed_commands;

        std::vector<std::map <std::string, std::string> >
            _client_output_buffer_limit;

        std::string _redis_config;

        std::string _get_string_value(std::string key);

        int _get_int_value(std::string key);

    public:
        Config(const boost::optional<std::string> & config=boost::none);

        std::string get_pidfile();

        int get_port();

        std::string get_log_file();

        std::string get_db_filename();

        std::string get_db_dir();

        std::string get_max_memory();

        std::string get_require_pass();

        std::string get_append_filename();
};


}}//end nova::redis
#endif /* CONFIG_H */
