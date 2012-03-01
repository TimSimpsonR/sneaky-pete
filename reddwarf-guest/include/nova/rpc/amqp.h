#ifndef __NOVA_RCC_AMQP_H
#define __NOVA_RPC_AMQP_H

#include "nova/rpc/amqp_ptr.h"
#include <stdlib.h>
#include <stdint.h>
extern "C" {
    #include <amqp.h>
    #include <amqp_framing.h>
}
#include "nova/Log.h"
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
                UNEXPECTED_FRAME_PAYLOAD_METHOD,
                WAIT_FRAME_FAILED
            };

            AmqpException(Code code) throw();

            virtual ~AmqpException() throw();

            const Code code;

            virtual const char * what() const throw();

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
            /** Attempts to declare the exchange. Does nothing if it fails. */
            void attempt_declare_exchange(const char * exchange_name,
                                          const char * type);

            /** Attempts to declare a queue. Does nothing if it fails. */
            void attempt_declare_queue(const char * queue_name,
                                       bool exclusive=false,
                                       bool auto_delete=false);

            static AmqpConnectionPtr create(const char * host_name, const int port,
                                            const char * user_name,
                                            const char * password,
                                            size_t client_memory);

            AmqpChannelPtr new_channel();

            void close();

            inline amqp_connection_state_t get_connection() {
                return connection;
            }

        protected:
            AmqpConnection(const char * host_name, const int port,
                           const char * user_name, const char * password,
                           size_t client_memory);

            ~AmqpConnection();

            static void check_references(AmqpConnection * connection);

            /* When certain operations such as declaring an exchange or queue
             * fail, the channel seems to be bad and attempts to use it again
             * result in the program hanging. This occurs even if the channel
             * is closed and re-opened. This is a kludge but for now the
             * solution is to mark those channels as bad. This means a program
             * could eventually run out of channels; however, this will only
             * happen if the exceptions are caught everywhere and execution
             * proceeds. Right now the
             * only exceptions worth catching are the ones from the declare
             * methods- if a program only catches a few throughout its lifetime,
             * this is workable. */
            void mark_channel_as_bad(AmqpChannel * channel);

            int new_channel_number() const;

            /** Closes and deletes channel. */
            void remove_channel(AmqpChannel * channel);

        private:
            AmqpConnection(const AmqpConnection &);
            AmqpConnection & operator = (const AmqpConnection &);

            std::vector<int> bad_channels;
            std::vector<AmqpChannel *> channels;
            amqp_connection_state_t connection;
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

            // Types are 'direct', 'topic'.
            void declare_exchange(const char * exchange_name,
                                  const char * type, bool passive=false);

            void declare_queue(const char * queue_name, bool passive=false);

            inline int get_channel_number() const {
                return channel_number;
            }

            AmqpQueueMessagePtr get_message(const char * queue_name);

            void publish(const char * exchange_name, const char * routing_key,
                         const char * messagebody);

        protected:
            AmqpChannel(AmqpConnection * parent, const int channel_number);
            ~AmqpChannel();

        private:
            AmqpChannel(const AmqpChannel &);
            AmqpChannel & operator = (const AmqpChannel &);

            const int channel_number;

            void check(const amqp_rpc_reply_t reply,
                       const AmqpException::Code & code);

            bool is_open;

            AmqpConnection * parent;

            int reference_count;

            void _throw(const AmqpException::Code & code);
    };

} } // end namespace

#endif
