#ifndef __NOVA_RPC_SENDER_H
#define __NOVA_RPC_SENDER_H

#include "nova/rpc/amqp_ptr.h"
#include <json/json.h>
#include "nova/guest/guest.h"
#include "nova/Log.h"
#include <memory>
#include <boost/optional.hpp>
#include <string>
#include <boost/utility.hpp>
#include <boost/smart_ptr.hpp>

namespace nova { namespace rpc {

    class Sender : boost::noncopyable  {
        public:
            Sender(AmqpConnectionPtr connection, const char * topic);

            ~Sender();

            void send(const JsonObject & object);

            void send(const char * publish_string);

        private:
            Sender(const Sender &);
            Sender & operator = (const Sender &);

            AmqpChannelPtr exchange;
            std::string exchange_name;
            const std::string queue_name;
            const std::string routing_key;
    };


    class ResilientSender {
        public:
            ResilientSender(const char * host, int port, const char * userid,
                            const char * password, size_t client_memory, const char * topic,
                            const char * exchange_name, unsigned long reconnect_wait_time);

            ~ResilientSender();

            void send(const JsonObject & publish_object);

            void send(const char * publish_string);

        private:
            ResilientSender(const ResilientSender &);
            ResilientSender & operator = (const ResilientSender &);

            void reset();

            size_t client_memory;

            void close();

            std::string exchange_name;

            std::string host;

            void open(bool wait_first);

            std::string password;

            int port;

            std::auto_ptr<Sender> sender;

            std::string topic;

            std::string userid;

            unsigned long reconnect_wait_time;

            boost::mutex conductor_mutex;

    };

    typedef boost::shared_ptr<ResilientSender> ResilientSenderPtr;

} }  // end namespace

#endif
