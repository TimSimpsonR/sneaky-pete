#ifndef CLIENT_H
#define CLIENT_H

#include <boost/optional.hpp>
#include <string>

#include "response.h"
#include "commands.h"
#include "control.h"
#include "config.h"


namespace nova { namespace redis {


class Client


    /*
     * This is the redis Client class it handles all communication
     * and process control of a redis instance.
     * This object is thread safe and handles closing
     * of any open file descriptors or sockets on deconstruction.
     */
{
    private:

        bool _authed;

        bool _name_set;

        int _socket;

        std::string _port;

        std::string _host;

        std::string _client_name;

        std::string _config_file;

        std::string _config_command;

        Commands*_commands;

        /*
         * Connects to the redis server using nova::redis::get_socket.
         * found
         */
        Response _connect();

        Response _send_redis_message(std::string message);

        Response _get_redis_response();

        Response _set_client();

        Response _auth();

        Response _reconnect();

        void _find_config_command();

    public:

        Control* control;

        Config* config;

        Client(const boost::optional<std::string> & host=boost::none,
               const boost::optional<std::string> & port=boost::none,
               const boost::optional<std::string> & client_name=boost::none,
               const boost::optional<std::string> & config_file=boost::none);

        ~Client();

        Response ping();

        Response bgsave();

        Response save();

        Response last_save();

        Response config_get(std::string name);

        Response config_set(std::string name, std::string value);

        Response config_rewrite();

};


}}//end nova::redis namespace
#endif /* CLIENT_H */
