#ifndef CLIENT_H
#define CLIENT_H

#include <boost/optional.hpp>
#include <string>

#include "response.h"
#include "commands.h"
#include <memory>
#include "config.h"
#include "connection.h"

namespace nova { namespace redis {


class Client


    /*
     * This is the redis Client class it handles all communication
     * and process control of a redis instance.
     * This object is thread safe and handles closing
     * of any open file descriptors or sockets on deconstruction.
     */
{
    public:

        Client(const boost::optional<std::string> & host=boost::none,
               const boost::optional<int> & port=boost::none,
               const boost::optional<std::string> & client_name=boost::none,
               const boost::optional<std::string> & config_file=boost::none);

        ~Client();

        Response ping();

        Response info();

        Response bgsave();

        Response save();

        Response last_save();

        Response config_get(std::string name);

        Response config_set(std::string name, std::string value);

        Response config_rewrite();

    private:

        bool _authed;

        bool _auth_required();

        const std::string _client_name;

        const std::string _config_file;

        std::unique_ptr<Config> config;

        std::unique_ptr<Commands> _commands;

        bool _name_set;

        Socket _socket;

        /*
         * Connects to the redis server using nova::redis::get_socket.
         * found
         */
        void _connect();

        Response _send_redis_message(std::string message);

        Response _get_redis_response();

        void _set_client();

        void _auth();

        void _reconnect();
};


}}//end nova::redis namespace
#endif /* CLIENT_H */
