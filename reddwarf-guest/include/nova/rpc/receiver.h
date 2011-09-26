#ifndef __NOVA_RPC_RECEIVER_H
#define __NOVA_RPC_RECEIVER_H

#include "nova/rpc/amqp_ptr.h"
#include <json/json.h>
#include "nova/log.h"
#include <string>


namespace nova { namespace rpc {

    class Receiver {

    public:
        Receiver(const char * host, int port,
                 const char * user_name, const char * password,
                 const char * queue_name);

        ~Receiver();

        /** Finishes a message. */
        void finish_message(json_object * arguments, json_object * output);

        /** Grabs the next message. Caller is responsible for calling ack. */
        json_object * next_message();

    private:
        AmqpConnectionPtr connection;
        int last_delivery_tag;
        Log log;
        AmqpChannelPtr queue;
        const std::string queue_name;

    };

} } // end namespace

#endif
