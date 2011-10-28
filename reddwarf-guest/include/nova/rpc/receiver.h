#ifndef __NOVA_RPC_RECEIVER_H
#define __NOVA_RPC_RECEIVER_H

#include "nova/rpc/amqp_ptr.h"
#include <nova/json.h>
#include "nova/Log.h"
#include <string>


namespace nova { namespace rpc {

    class Receiver {

    public:
        Receiver(const char * host, int port,
                 const char * user_name, const char * password,
                 const char * queue_name);

        ~Receiver();

        /** Finishes a message. */
        void finish_message(nova::JsonObjectPtr arguments,
                            nova::JsonObjectPtr output);

        /** Grabs the next message. Caller is responsible for calling ack. */
        nova::JsonObjectPtr next_message();

    private:
        AmqpConnectionPtr connection;
        int last_delivery_tag;
        Log log;
        AmqpChannelPtr queue;
        const std::string topic;

    };

} } // end namespace

#endif
