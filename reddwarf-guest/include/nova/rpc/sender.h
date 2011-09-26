#ifndef __NOVA_RPC_SENDER_H
#define __NOVA_RPC_SENDER_H

#include "nova/rpc/amqp_ptr.h"
#include <json/json.h>
#include "nova/log.h"

namespace nova { namespace rpc {

    class Sender {
        public:
            Sender(const char * host_name, int port,
                   const char * user_name, const char * password,
                   const char * exchange_name, const char * queue_name,
                   const char * routing_key);

            ~Sender();

            void send(json_object * object);

            void send(const char * publish_string);

        private:
            AmqpChannelPtr exchange;
            const std::string exchange_name;
            Log log;
            AmqpChannelPtr queue;
            const std::string queue_name;
            const std::string routing_key;
    };

} }  // end namespace

#endif
