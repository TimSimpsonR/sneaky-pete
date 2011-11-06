#ifndef __NOVA_RPC_RECEIVER_H
#define __NOVA_RPC_RECEIVER_H

#include "nova/rpc/amqp_ptr.h"
#include <nova/json.h>
#include "nova/guest/guest.h"
#include "nova/Log.h"
#include <boost/optional.hpp>
#include <string>


namespace nova { namespace rpc {

    class Receiver {

    public:
        Receiver(AmqpConnectionPtr connection, const char * topic);

        ~Receiver();

        /** Finishes a message. */
        void finish_message(const nova::guest::GuestOutput & output);

        /** Grabs the next message. */
        nova::guest::GuestInput next_message();

    private:
        AmqpConnectionPtr & connection;
        int last_delivery_tag;
        boost::optional<std::string> last_msg_id;
        Log log;
        AmqpChannelPtr queue;
        const std::string topic;

        nova::JsonObjectPtr _next_message();

    };

} } // end namespace

#endif
