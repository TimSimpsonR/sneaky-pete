#ifndef __NOVA_RPC_SENDER_H
#define __NOVA_RPC_SENDER_H

#include "nova/rpc/amqp_ptr.h"
#include <json/json.h>
#include "nova/guest/guest.h"
#include "nova/Log.h"
#include <memory>
#include <boost/optional.hpp>
#include "ResilientConnection.h"
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


    class ResilientSender : public ResilientConnection {
        public:
            ResilientSender(const char * host, int port, const char * userid,
                            const char * password, size_t client_memory,
                            const char * topic,
                            const char * exchange_name,
                            const char * instance_id,
                         const std::vector<unsigned long> reconnect_wait_times);

            virtual ~ResilientSender();

            /**
             *  Sends a message. Accepts JSON object element key value pairs
             *  as arguments, similar to nova::json_obj.
             *  These are added to an object sender crafts itself which
             *  includes the time sent and instance ID.
             */
            template<typename... Types>
            void send(const char * method, const Types... args) {
                nova::JsonObjectBuilder args_object;
                args_object.add(args...);
                send(method, args_object);
            }

            void send(const char * method, nova::JsonObjectBuilder & args);

            /**
             * Sends a message. This is mainly used for tests and the like.
             * Prefer the plain "send" message for most tasks.
             */
            void send_plain_string(const char * publish_string);

        protected:
            virtual void close();

            virtual void finish_open(AmqpConnectionPtr connection);

            virtual bool is_open() const;

        private:
            ResilientSender(const ResilientSender &);
            ResilientSender & operator = (const ResilientSender &);

            std::string exchange_name;

            std::string instance_id;

            std::auto_ptr<Sender> sender;

            std::string topic;

            boost::mutex conductor_mutex;

    };

    typedef boost::shared_ptr<ResilientSender> ResilientSenderPtr;

} }  // end namespace

#endif
