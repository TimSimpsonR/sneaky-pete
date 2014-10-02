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
#include <boost/tuple/tuple.hpp>
#include <boost/utility.hpp>


namespace nova { namespace rpc {

    /* Handles some state around receiving and responding to a message. By
     * storing the position it was at when an exception was thrown the
     * finish_message method can resume replying if an exception is thrown
     * in the middle of one of the Amqp actions. */
    class MessageState {
    public:
        MessageState();

        MessageState(const MessageState & other);

        /** Can be called multiple times. Does nothing if the message has
         *  already been replied to. */
        void finish_message(AmqpConnectionPtr connection,
                            AmqpChannelPtr queue, const std::string & msg);

        /** Sets a new message to reply to. */
        void set_response_info(int delivery_tag,
                               boost::optional<std::string> msg_id);

    private:
        int delivery_tag;
        boost::optional<std::string> msg_id;
        int must_send_reply_body;
        int must_send_reply_end;
    };

    class Receiver : boost::noncopyable  {

    public:
        Receiver(AmqpConnectionPtr connection, const char * topic,
                 const char * exchange_name,
                 MessageState msg=MessageState());

        ~Receiver();

        /** Finishes a message. */
        void finish_message(const nova::guest::GuestOutput & output);

        MessageState get_message_state() const;

        static void init_input_with_json(nova::guest::GuestInput & input,
                                         nova::JsonObject & msg);

        /** Grabs the next message. */
        nova::guest::GuestInput next_message();

    private:
        Receiver(const Receiver &);
        Receiver & operator = (const Receiver &);

        AmqpConnectionPtr connection;
        MessageState msg_state;
        AmqpChannelPtr queue;
        const std::string topic;

        boost::tuple<nova::JsonObjectPtr, int> _next_message();

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

        MessageState msg_state;

        std::auto_ptr<Receiver> receiver;

        std::string topic;
    };

} } // end namespace

#endif
