#ifndef __NOVA_RPC_RECEIVER_H
#define __NOVA_RPC_RECEIVER_H

#include "nova/rpc/amqp_ptr.h"
#include <nova/json.h>
#include "nova/guest/guest.h"
#include "nova/Log.h"
#include <memory>
#include <boost/optional.hpp>
#include <string>


namespace nova { namespace rpc {

    class Receiver {

    public:
        Receiver(AmqpConnectionPtr connection, const char * topic,
                 const char * exchange_name);

        ~Receiver();

        /** Finishes a message. */
        void finish_message(const nova::guest::GuestOutput & output);

        /** Grabs the next message. */
        nova::guest::GuestInput next_message();

    private:
        AmqpConnectionPtr connection;
        int last_delivery_tag;
        boost::optional<std::string> last_msg_id;
        Log log;
        AmqpChannelPtr queue;
        const std::string topic;

        nova::JsonObjectPtr _next_message();

    };

    /** Like the standard receiver, but kills and waits to restablish
     *  the connection anytime there's a problem. */
    class ResilentReceiver {

    public:
        ResilentReceiver(const char * host, int port, const char * userid,
            const char * password, size_t client_memory, const char * topic,
            const char * exchange_name, unsigned long reconnect_wait_time);

        ~ResilentReceiver();

        /** Finishes a message. */
        void finish_message(const nova::guest::GuestOutput & output);

        /** Grabs the next message. */
        nova::guest::GuestInput next_message();

        void reset();

    private:

        size_t client_memory;

        void close();

        std::string exchange_name;

        std::string host;

        Log log;

        void open(bool wait_first);

        std::string password;

        int port;

        std::auto_ptr<Receiver> receiver;

        std::string topic;

        std::string userid;

        unsigned long reconnect_wait_time;

    };

} } // end namespace

#endif
