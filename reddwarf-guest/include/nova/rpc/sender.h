#ifndef __NOVA_RPC_SENDER_H
#define __NOVA_RPC_SENDER_H

#include "nova/rpc/amqp_ptr.h"
#include <json/json.h>
#include "nova/Log.h"

namespace nova { namespace rpc {

    class Sender {
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

} }  // end namespace

#endif
