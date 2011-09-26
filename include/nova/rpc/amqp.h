#ifndef __NOVA_RCC_AMQP_H
#define __NOVA_RPC_AMQP_H

#include "nova/rpc/amqp_ptr.h"
#include <stdlib.h>
#include <stdint.h>
extern "C" {
    #include <amqp.h>
    #include <amqp_framing.h>
}
#include "nova/log.h"
#include <memory>
#include <vector>

namespace nova { namespace rpc {

    /** An exception relating to AMQP. */
    class AmqpException : public std::exception {

        public:
            enum Code {
                BODY_EXPECTED,
                BODY_LARGER,
                CLOSE_CHANNEL_FAILED,
                CLOSE_CONNECTION_FAILED,
                CONSUME,
                DESTROY_CONNECTION,
                BIND_QUEUE_FAILURE,
                CONNECTION_FAILED,
                DECLARE_QUEUE_FAILURE,
                EXCHANGE_DECLARE_FAIL,
                HEADER_EXPECTED,
                LOGIN_FAILED,
                OPEN_CHANNEL_FAILED,
                PUBLISH_FAILURE,
                WAIT_FRAME_FAILED
            };

            AmqpException(Code code) throw();

            virtual ~AmqpException() throw();

            virtual const char * what() const throw();

        private:

            Code code;
    };

    /** Throws an exception with the given code if the reply is not normal. */
    extern void amqp_check(const amqp_rpc_reply_t reply,
                           const AmqpException::Code & code);

    /** Manages a connection to Amqp as well as all open channels. */
    class AmqpConnection {
        friend class AmqpChannel;
        friend void intrusive_ptr_add_ref(AmqpConnection * ref);
        friend void intrusive_ptr_release(AmqpConnection * ref);
        friend void intrusive_ptr_release(AmqpChannel * ref);

        public:
            static AmqpConnectionPtr create(const char * host_name, const int port,
                                            const char * user_name,
                                            const char * password);
            AmqpChannelPtr new_channel();

            void close();

            inline amqp_connection_state_t get_connection() {
                return connection;
            }

        protected:
            AmqpConnection(const char * host_name, const int port,
                           const char * user_name, const char * password);

            ~AmqpConnection();

            static void check_references(AmqpConnection * connection);

            int new_channel_number() const;

            /** Closes and deletes channel. */
            void remove_channel(AmqpChannel * channel);

        private:
            std::vector<AmqpChannel *> channels;
            amqp_connection_state_t connection;
            Log log;
            int reference_count;
            int sockfd;
    };


    struct AmqpQueueMessage {
        AmqpQueueMessage();
        std::string content_type;
        int delivery_tag;
        std::string exchange;
        std::string message;
        std::string routing_key;
    };

    typedef boost::shared_ptr<AmqpQueueMessage> AmqpQueueMessagePtr;

    /** Manages a channel to amqp. */
    class AmqpChannel {
        friend void intrusive_ptr_add_ref(AmqpChannel * ref);
        friend void intrusive_ptr_release(AmqpChannel * ref);
        friend class AmqpConnection;

        public:

            void ack_message(int delivery_tag);

            void bind_queue_to_exchange(const char * queue_name,
                                            const char * exchange_name,
                                            const char * routing_key);

            void close();

            void declare_exchange(const char * exchange_name);

            void declare_queue(const char * queue_name);

            inline int get_channel_number() const {
                return channel_number;
            }

            AmqpQueueMessagePtr get_message(const char * queue_name);

            void publish(const char * exchange_name, const char * routing_key,
                         const char * messagebody);

        protected:
            AmqpChannel(AmqpConnection * parent, const int channel_number);
            AmqpChannel(const AmqpChannel & other);
            ~AmqpChannel();

        private:
            const int channel_number;
            bool is_open;
            AmqpConnection * parent;
            int reference_count;
    };

} } // end namespace

#endif
