#ifndef __NOVA_RPC_RECEIVER_H
#define __NOVA_RPC_RECEIVER_H

#include "nova/rpc/amqp_ptr.h"
#include <nova/json.h>
#include "nova/guest/guest.h"
#include "nova/Log.h"
#include <memory>
#include <boost/optional.hpp>
#include "ResilientConnection.h"
#include <string>
#include <boost/utility.hpp>


namespace nova { namespace rpc {

    class Receiver : boost::noncopyable  {

    public:
        Receiver(AmqpConnectionPtr connection, const char * topic,
                 const char * exchange_name);

        ~Receiver();

        /** Finishes a message. */
        void finish_message(const nova::guest::GuestOutput & output);

        static void init_input_with_json(nova::guest::GuestInput & input,
                                         nova::JsonObject & msg);

        /** Grabs the next message. */
        nova::guest::GuestInput next_message();

    private:
        Receiver(const Receiver &);
        Receiver & operator = (const Receiver &);

        AmqpConnectionPtr connection;
        int last_delivery_tag;
        boost::optional<std::string> last_msg_id;
        AmqpChannelPtr queue;
        const std::string topic;

        nova::JsonObjectPtr _next_message();

    };

    /** Like the standard receiver, but kills and waits to restablish
     *  the connection anytime there's a problem. */
    class ResilientReceiver : public ResilientConnection {

    public:
        ResilientReceiver(const char * host, int port, const char * userid,
            const char * password, size_t client_memory, const char * topic,
            const char * exchange_name,
            std::vector<unsigned long> reconnect_wait_times);

        ~ResilientReceiver();

        /** Finishes a message. */
        void finish_message(const nova::guest::GuestOutput & output);

        /** Grabs the next message. */
        nova::guest::GuestInput next_message();

    protected:
        virtual void close();

        virtual void finish_open(AmqpConnectionPtr connection);

        virtual bool is_open() const;

    private:
        ResilientReceiver(const ResilientReceiver &);
        ResilientReceiver & operator = (const ResilientReceiver &);

        std::string exchange_name;

        std::auto_ptr<Receiver> receiver;

        std::string topic;
    };

} } // end namespace

#endif
