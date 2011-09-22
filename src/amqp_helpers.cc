#include "amqp_helpers.h"
#include <algorithm>
#include <boost/foreach.hpp>
#include <limits>
#include <sstream>

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
                               const char * user_name, const char * password)
: channels(), connection(0), log(), reference_count(0), sockfd(-1)
{
    // Create connection.
    connection = amqp_new_connection();
    sockfd = amqp_open_socket(host_name, port);
    if (sockfd < 0) {
        throw AmqpException(AmqpException::CONNECTION_FAILED);
    }
    amqp_set_sockfd(connection, sockfd);

    // Login
    amqp_check(amqp_login(connection, "/", 0, 131072, 0,
                     AMQP_SASL_METHOD_PLAIN, user_name, password),
               AmqpException::LOGIN_FAILED);

}

AmqpConnection::~AmqpConnection() {
    try {
        close();
    } catch(const AmqpException & ae) {
        log.error("Destructor called close and caught AMQP error!");
        log.error(ae.what());
    } catch(const std::exception & ex) {
        log.error("Destructor called close and caught error!");
        log.error(ex.what());
    }
}

void AmqpConnection::check_references(AmqpConnection * ref) {
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
                                         const char * password) {
    AmqpConnectionPtr ptr(new AmqpConnection(host_name, port,
                                             user_name, password));
    return ptr;
}

int AmqpConnection::new_channel_number() const {
    bool found=false;
    int number = 10;
    while(!found) {
        found = true;
        BOOST_FOREACH(const AmqpChannel * const channel, channels) {
            if (channel->get_channel_number() == number) {
                found = false;
                number ++;
                break;
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
            channel->close();
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
        ref->parent->remove_channel(ref);
    }
}

AmqpChannel::AmqpChannel(AmqpConnection * parent, const int channel_number)
: channel_number(channel_number), is_open(false), parent(parent),
  reference_count(0)
{
    amqp_connection_state_t conn = parent->get_connection();
    amqp_channel_open(conn, channel_number);
    amqp_check(amqp_get_rpc_reply(conn), AmqpException::OPEN_CHANNEL_FAILED);
    is_open = true;
}

AmqpChannel::~AmqpChannel() {
    try {
        close();
    } catch(const std::exception & ex) {
        parent->log.error("Channel destructor called close and caught error!");
        parent->log.error(ex.what());
    }
}

void AmqpChannel::ack_message(int delivery_tag) {
    amqp_connection_state_t conn = parent->get_connection();
    amqp_basic_ack(conn, channel_number, delivery_tag, 0);
}

void AmqpChannel::close() {
    if (is_open) {
        amqp_connection_state_t conn = parent->get_connection();
        amqp_check(amqp_channel_close(conn, channel_number, AMQP_REPLY_SUCCESS),
                   AmqpException::CLOSE_CHANNEL_FAILED);
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
    amqp_check(reply, AmqpException::BIND_QUEUE_FAILURE);
}

void AmqpChannel::declare_exchange(const char * exchange_name) {
    // Declare a direct exchange.
    //(amqp_channel_t)
    amqp_connection_state_t conn = parent->get_connection();
    amqp_exchange_declare(conn, channel_number,
                          amqp_cstring_bytes(exchange_name),
                          amqp_cstring_bytes("direct"),
                          0, 0, AMQP_EMPTY_TABLE);
    amqp_check(amqp_get_rpc_reply(conn), AmqpException::EXCHANGE_DECLARE_FAIL);
}

void AmqpChannel::declare_queue(const char * queue_name) {
    amqp_connection_state_t conn = parent->get_connection();
    // Declare a queue
    amqp_queue_declare_t args;
    args.ticket = 0;
    args.queue = amqp_cstring_bytes(queue_name);
    args.passive = 0;
    args.durable = 0;
    args.exclusive = 0;
    args.auto_delete = 0;
    args.nowait = 0;
    args.arguments = AMQP_EMPTY_TABLE;
    amqp_method_number_t number = AMQP_QUEUE_DECLARE_OK_METHOD;
    amqp_rpc_reply_t reply = amqp_simple_rpc(conn, channel_number,
                                            AMQP_QUEUE_DECLARE_METHOD,
                                            &number,
                                            &args);
    amqp_check(reply, AmqpException::DECLARE_QUEUE_FAILURE);
    if (reply.reply.id != AMQP_QUEUE_DECLARE_OK_METHOD) {
        throw AmqpException(AmqpException::DECLARE_QUEUE_FAILURE);
    }
}

AmqpQueueMessagePtr AmqpChannel::get_message(const char * queue_name) {
    amqp_connection_state_t conn = parent->get_connection();
    amqp_basic_consume(conn, channel_number,
                       amqp_cstring_bytes(queue_name),
                       AMQP_EMPTY_BYTES,
                       0, 0, 0, AMQP_EMPTY_TABLE);

    amqp_check(amqp_get_rpc_reply(conn), AmqpException::CONSUME);
    // use select to not block
    amqp_frame_t frame;

    //
    amqp_maybe_release_buffers(conn);
    int result = amqp_simple_wait_frame(conn, &frame);

    AmqpQueueMessagePtr rtn;
    if (result < 0) {
        return rtn;
    }
    if (frame.payload.method.id != AMQP_BASIC_DELIVER_METHOD) {
        return rtn;
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
    props.content_type = amqp_cstring_bytes("text/text");
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
