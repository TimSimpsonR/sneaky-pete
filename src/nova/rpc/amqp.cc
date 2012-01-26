#include "nova/rpc/amqp.h"
#include <algorithm>
#include <boost/foreach.hpp>
//#include "nova/utils/io.h"
#include <limits>
#include <sstream>
#include <string.h>

// For SIGPIPE ignoring
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>


namespace nova { namespace rpc {


/**---------------------------------------------------------------------------
 *- AmqpException
 *---------------------------------------------------------------------------*/

AmqpException::AmqpException(AmqpException::Code code) throw()
: code(code) {
}

AmqpException::~AmqpException() throw() {
}

const char * AmqpException::what() const throw() {
    switch(code) {
        case BIND_QUEUE_FAILURE:
            return "Bind queue failure.";
        case BODY_LARGER:
            return "The body returned in the message was larger than stated "
                   "in the header.";
        case BODY_EXPECTED:
            return "Body expected in message but unseen!";
        case CLOSE_CHANNEL_FAILED:
            return "Could not close channel.";
        case CLOSE_CONNECTION_FAILED:
            return "Could not close connection.";
        case CONNECTION_FAILED:
            return "Connection failed.";
        case CONSUME:
            return "Consume failed.";
        case DECLARE_QUEUE_FAILURE:
            return "Failed to declare queue.";
        case DESTROY_CONNECTION:
            return "Failed to destroy connection.";
        case EXCHANGE_DECLARE_FAIL:
            return "Exchange declare fail.";
        case HEADER_EXPECTED:
            return "A header was expected in a message but unseen!";
        case LOGIN_FAILED:
            return "Login failed!";
        case OPEN_CHANNEL_FAILED:
            return "Failed to open channel.";
        case PUBLISH_FAILURE:
            return "Error publishing message.";
        case UNEXPECTED_FRAME_PAYLOAD_METHOD:
            return "Did not expect to see any frame other than "
                   "AMQP_BASIC_DELIVER_METHOD at this time.";
        case WAIT_FRAME_FAILED:
            return "Error while waiting for the next frame of a message.";
        default:
            return "General failure.";
    }
}

void amqp_check(const amqp_rpc_reply_t reply,
                const AmqpException::Code & code) {
    if (reply.reply_type != AMQP_RESPONSE_NORMAL) {
        throw AmqpException(code);
    }
}


/**---------------------------------------------------------------------------
 *- AmqpConnection
 *---------------------------------------------------------------------------*/

void intrusive_ptr_add_ref(AmqpConnection * ref) {
    ref->reference_count ++;
}

void intrusive_ptr_release(AmqpConnection * ref) {
    ref->reference_count --;
    AmqpConnection::check_references(ref);
}

AmqpConnection::AmqpConnection(const char * host_name, const int port,
                               const char * user_name, const char * password,
                               size_t client_memory)
: bad_channels(), channels(), connection(0), reference_count(0),
  sockfd(-1)
{
    // Create connection.
    connection = amqp_new_connection();
    try {
        sockfd = amqp_open_socket(host_name, port);
        if (sockfd < 0) {
            throw AmqpException(AmqpException::CONNECTION_FAILED);
        }
        //int set = 1;
        //setsockopt(sockfd, SOL_SOCKET, SO_NOSIGPIPE, (void *)&set, sizeof(int));

        amqp_set_sockfd(connection, sockfd);

        // Login
        amqp_check(amqp_login(connection, "/", 0, client_memory, 0,
                              AMQP_SASL_METHOD_PLAIN, user_name, password),
                   AmqpException::LOGIN_FAILED);
    } catch(const AmqpException & amqpe) {
        if (amqp_destroy_connection(connection) < 0) {
            NOVA_LOG_ERROR("FATAL ERROR: COULD NOT DESTROY OPEN AMQP CONNECTION!");
        }
        throw;
    }

}

AmqpConnection::~AmqpConnection() {
    try {
        close();
    } catch(const AmqpException & ae) {
        NOVA_LOG_ERROR("Destructor called close and caught AMQP error!");
        NOVA_LOG_ERROR(ae.what());
    } catch(const std::exception & ex) {
        NOVA_LOG_ERROR("Destructor called close and caught error!");
        NOVA_LOG_ERROR(ex.what());
    }
}

void AmqpConnection::check_references(AmqpConnection * ref) {
    NOVA_LOG_DEBUG2("Checking the references to AmqpConnection, which has "
                    "a reference_count of %d and owns %d channels.",
                    ref->reference_count, ref->channels.size());
    if (ref->reference_count <= 0 && ref->channels.size() <= 0) {
        delete ref;
    }
}

void AmqpConnection::close() {
    amqp_check(amqp_connection_close(connection, AMQP_REPLY_SUCCESS),
               AmqpException::CLOSE_CONNECTION_FAILED);
    if (amqp_destroy_connection(connection) < 0) {
        throw AmqpException(AmqpException::DESTROY_CONNECTION);
    }
}

AmqpConnectionPtr AmqpConnection::create(const char * host_name, const int port,
                                         const char * user_name,
                                         const char * password,
                                         size_t client_memory) {
    AmqpConnectionPtr ptr(new AmqpConnection(host_name, port,
                                             user_name, password,
                                             client_memory));
    return ptr;
}

void AmqpConnection::attempt_declare_exchange(const char * exchange_name,
                                              const char * type) {
    AmqpChannelPtr channel = new_channel();
    NOVA_LOG_INFO2("Attempting to declare exchange %s.", exchange_name);
    try {
        channel->declare_exchange(exchange_name, type, true);
        NOVA_LOG_INFO("Exchange already declared.");
    } catch(const AmqpException & ae) {
        if (ae.code == AmqpException::EXCHANGE_DECLARE_FAIL) {
            NOVA_LOG_INFO("Could not passive declare exchange. Trying non-passive.");
            new_channel()->declare_exchange(exchange_name, type, false);
        } else {
            NOVA_LOG_ERROR("AmqpException was thrown trying to declare exchange.");
            throw ae;
        }
    }
}

void AmqpConnection::attempt_declare_queue(const char * queue_name,
                                           bool exclusive,
                                           bool auto_delete) {
    AmqpChannelPtr channel = new_channel();
    NOVA_LOG_INFO2("Attempting to declare queue %s.", queue_name);
    try {
        channel->declare_queue(queue_name, true);
        NOVA_LOG_INFO("Queue already declared.");
    } catch(const AmqpException & ae) {
        if (ae.code == AmqpException::DECLARE_QUEUE_FAILURE) {
            NOVA_LOG_INFO2("Could not declare queue %s. Trying non-passive.",
                           queue_name);
            new_channel()->declare_queue(queue_name, false);
        } else {
            NOVA_LOG_ERROR("AmqpException was thrown trying to declare queue.");
            throw ae;
        }
    }
}

void AmqpConnection::mark_channel_as_bad(AmqpChannel * channel) {
    bad_channels.push_back(channel->get_channel_number());
}

int AmqpConnection::new_channel_number() const {
    bool found=false;
    int number = 10;
    while(!found) {
        found = true;
        BOOST_FOREACH(int bad_channel, bad_channels) {
            if (bad_channel == number) {
                found = false;
                number ++;
                break;
            }
        }
        if (found == true) {
            BOOST_FOREACH(const AmqpChannel * const channel, channels) {
                if (channel->get_channel_number() == number) {
                    found = false;
                    number ++;
                    break;
                }
            }
        }
    }
    return number;
}

AmqpChannelPtr AmqpConnection::new_channel() {
    AmqpChannel * new_instance = new AmqpChannel(this, new_channel_number());
    channels.push_back(new_instance);
    AmqpChannelPtr ptr(new_instance);
    return ptr;
}

void AmqpConnection::remove_channel(AmqpChannel * channel) {
    for (std::vector<AmqpChannel *>::iterator itr = channels.begin();
         itr != channels.end(); itr ++) {
        if (*itr == channel) {
            channels.erase(itr);
            // A try / catch is necessary here because this method is called by
            // the smart pointer release function, which could be used very
            // naturually in a destructor of some class.
            try {
                channel->close();
            } catch(const AmqpException & ae) {
                NOVA_LOG_ERROR2("AmqpException during channel close, removing "
                                "anyway. Exception message: ", ae.what());
            }
            delete channel;
            break;
        }
    }
    check_references(this);
}


/**---------------------------------------------------------------------------
 *- AmqpQueueMessage
 *---------------------------------------------------------------------------*/

AmqpQueueMessage::AmqpQueueMessage()
:  content_type(),
   delivery_tag(),
   exchange(),
   message(),
   routing_key()
{
}

/**---------------------------------------------------------------------------
 *- AmqpChannel
 *---------------------------------------------------------------------------*/

void intrusive_ptr_add_ref(AmqpChannel * ref) {
    ref->reference_count ++;
}

void intrusive_ptr_release(AmqpChannel * ref) {
    ref->reference_count --;
    if (ref->reference_count <= 0) {
        NOVA_LOG_DEBUG2("Releasing reference to channel # %d. "
                        "Reference count is now %d.",
                        ref->channel_number, ref->reference_count);
        ref->parent->remove_channel(ref);
    }
}

AmqpChannel::AmqpChannel(AmqpConnection * parent, const int channel_number)
: channel_number(channel_number), is_open(false), parent(parent),
  reference_count(0)
{
    amqp_connection_state_t conn = parent->get_connection();
    NOVA_LOG_DEBUG2("Opening new channel with # %d.", channel_number);
    amqp_channel_open(conn, channel_number);
    amqp_check(amqp_get_rpc_reply(conn), AmqpException::OPEN_CHANNEL_FAILED);
    is_open = true;
}

AmqpChannel::~AmqpChannel() {
    try {
        close();
    } catch(const std::exception & ex) {
        NOVA_LOG_ERROR("Channel destructor called close and caught error!");
        NOVA_LOG_ERROR(ex.what());
    }
}

void AmqpChannel::ack_message(int delivery_tag) {
    amqp_connection_state_t conn = parent->get_connection();
    amqp_basic_ack(conn, channel_number, delivery_tag, 0);
}

void AmqpChannel::check(const amqp_rpc_reply_t reply,
                             const AmqpException::Code & code) {
    if (reply.reply_type != AMQP_RESPONSE_NORMAL) {
        _throw(code);
    }
}

// void handler(int blah) {
//     Log log;
//     for (int i = 0; i < 25; i ++) {
//         log.error2("Handling Sig Pipe.");
//     }
// }

// void handler2(int signal_number, siginfo_t * info, void * context) {
//     Log log;
//     log.error("handler 2 was called.");
// }

void AmqpChannel::close() {
    if (is_open) {
        NOVA_LOG_DEBUG2("Closing channel #%d", channel_number);
        amqp_connection_state_t conn = parent->get_connection();

        //TODO(tim.simpson): I can't seem to block the SIGPIPE signal no matter
        // what I do. If I don't, this method crashes the program anytime
        // rabbit goes down. Right now I'm working around it by changing the
        // librabbit-c source but it needs to be fixed for real.
        // Unfortunately (and inexplicably) none of the code below (or other
        // variations which I haven't checked-in) will do the trick:

        ///* Start ignoring SIGPIPE */
        //signal(SIGPIPE, SIG_IGN);
        // struct sigaction action;
        // memset(&action, 0, sizeof(action));
        // action.sa_handler = handler; //SIG_IGN;
        // action.sa_flags = SA_SIGINFO;
        // action.sa_sigaction = handler2;
        // //act.sa_mask = 0;
        // //act.sa_flags = 0;
        // //sa_restorer = 0

        // parent->log.info("Turning off SIGPIPE.");
        // if ((sigemptyset(&action.sa_mask) != 0)
        //     || (sigaction(SIGPIPE, &action, NULL) != 0)) {
        //     parent->log.error2("Could not avoid SIGPIPE: %s", strerror(errno));
        // }

        const amqp_rpc_reply_t reply =
            amqp_channel_close(conn, channel_number, AMQP_REPLY_SUCCESS);

        /* End ignoring of SIGPIPEs */
        //signal(SIGPIPE, handler);
        //spa.end();
        //action.sa_handler = SIG_DFL;
        // parent->log.info("Turning SIGPIPE back on.");
        // if (sigaction(SIGPIPE, &action, NULL) != 0) {
        //     parent->log.error2("Error turning SIGPIPE on: %s", strerror(errno));
        // }

        check(reply, AmqpException::CLOSE_CHANNEL_FAILED);
        is_open = false;
    }
}

void AmqpChannel::bind_queue_to_exchange(const char * queue_name,
                                         const char * exchange_name,
                                         const char * routing_key) {
    amqp_connection_state_t conn = parent->get_connection();

    amqp_queue_bind_t args;
    args.ticket = 0;
    args.queue = amqp_cstring_bytes(queue_name);
    args.exchange = amqp_cstring_bytes(exchange_name);
    args.routing_key = amqp_cstring_bytes(routing_key);
    args.nowait = 0;
    args.arguments.num_entries = 0;
    args.arguments.entries = NULL;

    amqp_method_number_t number = AMQP_QUEUE_BIND_OK_METHOD;
    amqp_rpc_reply_t reply = amqp_simple_rpc(conn, channel_number,
                                             AMQP_QUEUE_BIND_METHOD,
                                             &number,
                                             &args);
    check(reply, AmqpException::BIND_QUEUE_FAILURE);
}

void AmqpChannel::declare_exchange(const char * exchange_name,
                                   const char * type, bool passive) {
    amqp_connection_state_t conn = parent->get_connection();
    amqp_exchange_declare_t args;
    args.exchange = amqp_cstring_bytes(exchange_name);
    args.type = amqp_cstring_bytes(type);
    args.passive = passive ? 1 : 0;
    args.durable = 0;
    args.auto_delete = 0;
    args.internal = 0;
    args.nowait = 0;
    args.arguments = AMQP_EMPTY_TABLE;
    amqp_method_number_t number = AMQP_EXCHANGE_DECLARE_OK_METHOD;
    amqp_rpc_reply_t reply = amqp_simple_rpc(conn, channel_number,
                                            AMQP_EXCHANGE_DECLARE_METHOD,
                                            &number,
                                            &args);
    check(reply, AmqpException::EXCHANGE_DECLARE_FAIL);
    if (reply.reply.id != AMQP_EXCHANGE_DECLARE_OK_METHOD) {
        parent->mark_channel_as_bad(this);
        throw AmqpException(AmqpException::EXCHANGE_DECLARE_FAIL);
    }
}

void AmqpChannel::declare_queue(const char * queue_name, bool passive) {
    const bool exclusive=false;
    const bool auto_delete=false;
    amqp_connection_state_t conn = parent->get_connection();
    // Declare a queue
    amqp_queue_declare_t args;
    args.ticket = 0;
    args.queue = amqp_cstring_bytes(queue_name);
    args.passive = passive ? 1 : 0;
    args.durable = 0;
    args.exclusive = exclusive ? 1 : 0;
    args.auto_delete = auto_delete ? 1 : 0;
    args.nowait = 0;
    args.arguments = AMQP_EMPTY_TABLE;
    amqp_method_number_t number = AMQP_QUEUE_DECLARE_OK_METHOD;
    amqp_rpc_reply_t reply = amqp_simple_rpc(conn, channel_number,
                                            AMQP_QUEUE_DECLARE_METHOD,
                                            &number,
                                            &args);
    check(reply, AmqpException::DECLARE_QUEUE_FAILURE);
    if (reply.reply.id != AMQP_QUEUE_DECLARE_OK_METHOD) {
        parent->mark_channel_as_bad(this);
        throw AmqpException(AmqpException::DECLARE_QUEUE_FAILURE);
    }
}

AmqpQueueMessagePtr AmqpChannel::get_message(const char * queue_name) {
    AmqpQueueMessagePtr no;

    amqp_connection_state_t conn = parent->get_connection();
    amqp_basic_consume(conn, channel_number,
                       amqp_cstring_bytes(queue_name),
                       AMQP_EMPTY_BYTES,
                       1, 0, 0, AMQP_EMPTY_TABLE);

    amqp_check(amqp_get_rpc_reply(conn), AmqpException::CONSUME);
    // use select to not block
    amqp_frame_t frame;

    //
    amqp_maybe_release_buffers(conn);
    int result = amqp_simple_wait_frame(conn, &frame);

    AmqpQueueMessagePtr rtn;

    if (result < 0) {
        NOVA_LOG_ERROR("Warning: amqp_simple_wait_frame returned < 0 result.")
        return rtn;
    }
    if (frame.payload.method.id != AMQP_BASIC_DELIVER_METHOD) {
        NOVA_LOG_ERROR2("Warning: amqp_simple_wait_frame returned frame whose "
                        "id was not AMQP_BASIC_DELIVER_METHOD, but %d.",
                        frame.payload.method.id);
        // I've seen in cases where an empty pointer is returned here that the
        // next message, when read, has a decoded pointer to 0x22 (not null,
        // but still garbage). So the best solution is throw an exception.
        // The resilent receiver will open a new connection and things will
        // proceed smoothly from there.
        throw AmqpException(AmqpException::UNEXPECTED_FRAME_PAYLOAD_METHOD);
    }
    amqp_basic_deliver_t * decoded = (amqp_basic_deliver_t *)
                                     frame.payload.method.decoded;
    rtn.reset(new AmqpQueueMessage());
    rtn->delivery_tag = decoded->delivery_tag;
    rtn->exchange.append((char *)decoded->exchange.bytes,
                  (size_t) decoded->exchange.len);
    rtn->routing_key.append((char *)decoded->routing_key.bytes,
                     (size_t) decoded->routing_key.len);

    if (amqp_simple_wait_frame(conn, &frame) < 0) {
        return rtn;
    }

    if (frame.frame_type != AMQP_FRAME_HEADER) {
        throw AmqpException(AmqpException::HEADER_EXPECTED);
    }
    amqp_basic_properties_t * properties = (amqp_basic_properties_t *)
                                           frame.payload.properties.decoded;
    if (properties->_flags & AMQP_BASIC_CONTENT_TYPE_FLAG) {
        rtn->content_type.append((char *) properties->content_type.bytes,
                                 (size_t) properties->content_type.len);
    }

    size_t body_target = frame.payload.properties.body_size;
    size_t body_received = 0;
    while (body_received < body_target) {
        result = amqp_simple_wait_frame(conn, &frame);
		if (result < 0) {
		  throw AmqpException(AmqpException::WAIT_FRAME_FAILED);
		}
		if (frame.frame_type != AMQP_FRAME_BODY) {
		    throw AmqpException(AmqpException::BODY_EXPECTED);
		}
		body_received += frame.payload.body_fragment.len;
		if (body_received > body_target) {
		    throw AmqpException(AmqpException::BODY_LARGER);
		}
		//TODO: Is it safe to assume these are normal chars?
		rtn->message.append((char *) frame.payload.body_fragment.bytes,
                      		(size_t) frame.payload.body_fragment.len);
    }
    return rtn;
}

void AmqpChannel::publish(const char * exchange_name,
                          const char * routing_key, const char * messagebody) {
    amqp_connection_state_t conn = parent->get_connection();
    amqp_basic_properties_t props;
    props._flags = AMQP_BASIC_CONTENT_TYPE_FLAG
                   | AMQP_BASIC_DELIVERY_MODE_FLAG
                   | AMQP_BASIC_CONTENT_ENCODING_FLAG;
    props.content_type = amqp_cstring_bytes("application/json"); //text/text");
    props.content_encoding = amqp_cstring_bytes("UTF-8");
    props.delivery_mode = 2; /* persistent delivery mode */
    int result = amqp_basic_publish(conn,
                    channel_number,
                    amqp_cstring_bytes(exchange_name),
                    amqp_cstring_bytes(routing_key),
                    1,
                    0,
                    &props,
                    amqp_cstring_bytes(messagebody));
    if (result < 0) {
        throw AmqpException(AmqpException::PUBLISH_FAILURE);
    }
}

void AmqpChannel::_throw(const AmqpException::Code & code) {
    parent->mark_channel_as_bad(this);
    throw AmqpException(code);
}

} } // end namespace
