#include "pch.hpp"
#include "nova/rpc/receiver.h"
#include "nova/rpc/amqp.h"

#include <boost/format.hpp>
#include <boost/thread.hpp>
#include "nova/guest/GuestException.h"
#include "nova/Log.h"
#include <string>
#include <sstream>

using boost::format;
using nova::guest::GuestInput;
using nova::guest::GuestException;
using nova::guest::GuestOutput;
using boost::optional;
using nova::json_string;
using nova::JsonObject;
using nova::JsonObjectPtr;
using nova::Log;
using std::string;

namespace nova { namespace rpc {

namespace {
    const char * END_MESSAGE = "{ \"failure\": null, \"result\":null, "
                               "  \"ending\":true }";
}

/**---------------------------------------------------------------------------
 *- MessageState
 *---------------------------------------------------------------------------*/

MessageState::MessageState()
:   delivery_tag(-1),
    msg_id(boost::none),
    must_send_reply_body(false),
    must_send_reply_end(false)
{}

MessageState::MessageState(const MessageState & other)
:   delivery_tag(other.delivery_tag),
    msg_id(other.msg_id),
    must_send_reply_body(other.must_send_reply_body),
    must_send_reply_end(other.must_send_reply_end)
{}

void MessageState::finish_message(AmqpConnectionPtr connection,
                                  AmqpChannelPtr queue,
                                  const string & msg) {
    if (delivery_tag != -1) {
        queue->ack_message(delivery_tag);
        delivery_tag = -1;
    }

    if (!msg_id) {
        // No reply necessary.
        NOVA_LOG_INFO("Acknowledged message but will not send reply because "
                      "no _msg_id was given.");
        return;
    }

    const char * const exchange_name = msg_id.get().c_str();
    const char * const routing_key = msg_id.get().c_str();

    AmqpChannelPtr rtn_ex_channel = connection->new_channel();
    if (must_send_reply_body -- > 0) {
        NOVA_LOG_INFO("Replying with 'body' message.");
        rtn_ex_channel->publish(exchange_name, routing_key, msg.c_str());
        must_send_reply_body = 0;
    }


    if (must_send_reply_end -- > 0) {
        // This is like telling Nova "roger."
        NOVA_LOG_INFO("Replying with 'end' message: %s", END_MESSAGE);
        rtn_ex_channel->publish(exchange_name, routing_key, END_MESSAGE);
        must_send_reply_end = 0;
    }

    msg_id = boost::none;
    // Manually close in order to throw any errors that might happen here.
    rtn_ex_channel->close();
}


void MessageState::set_response_info(int delivery_tag, optional<string> msg_id) {
    this->delivery_tag = delivery_tag;
    this->msg_id = msg_id;
    if (this->msg_id) {
        must_send_reply_body = 3;
        must_send_reply_end = 3;
    }
}

/**---------------------------------------------------------------------------
 *- Receiver
 *---------------------------------------------------------------------------*/

Receiver::Receiver(AmqpConnectionPtr connection, const char * topic,
                   const char * exchange_name, const MessageState msg)
:   connection(connection),
    msg_state(msg),
    queue(),
    topic(topic)
{
    queue = connection->new_channel();


    // Nova seems to declare the following
    // Queues:
    // "%s" % topic
    // "%s.%s" % (topic,hostname) - hostname is the instance ID
    // "%s_fanout_%s" % (topic, random int?)

    // Then it makes the following exchanges
    // "%s_fanout" % topic, 'fanout'
    // "nova" 'topic'
    // "nova" 'direct"'

    // TODO : Make three extra queues to listen to. :c
    const char * queue_name = topic;

    //MB queue->declare_queue(queue_name);

    connection->attempt_declare_queue(queue_name);
    connection->attempt_declare_exchange(exchange_name, "topic");

    //queue->declare_exchange(topic, "direct");  //TODO(tim.simpson): Remove?
    queue->bind_queue_to_exchange(queue_name, exchange_name, queue_name);
}

Receiver::~Receiver() {
}

string create_reply_message_string(const GuestOutput & output) {
    string msg;
    if (!output.failure) {
        msg = str(format("{ \"failure\":null, \"result\":%s }")
                  % output.result->to_string());
        JsonObject obj(msg.c_str());
        msg = obj.to_string();
    } else {
        msg = str(format("{\"failure\":{\"exc_type\":\"std::exception\", "
                         "\"value\":%s, "
                         "\"traceback\":\"unavailable\" } }")
                  % json_string(output.failure.get().c_str()));
    }
    if (msg.find("password") == string::npos) {
        NOVA_LOG_INFO("Replying with the following: %s", msg.c_str());
    } else {
        NOVA_LOG_INFO("Replying to message...");
        #ifdef _DEBUG
            NOVA_LOG_INFO("Showing the message because SP is in debug mode.");
            NOVA_LOG_INFO("(DEBUG) Replying with the following: %s",
                           msg.c_str());
        #endif
    }
    return msg;
}

void Receiver::finish_message(const GuestOutput & output) {
    const string msg_string = create_reply_message_string(output);
    msg_state.finish_message(connection, queue, msg_string);
}

MessageState Receiver::get_message_state() const {
    return msg_state;
}

boost::tuple<JsonObjectPtr, int> Receiver::_next_message() {
    AmqpQueueMessagePtr msg;
    while(!msg) {
        msg = queue->get_message(topic.c_str());
        if (!msg) {
            NOVA_LOG_INFO("Received an empty message.");
        }
    }
    std::stringstream log_msg;
    log_msg << "Received message "
        << ", key " << msg->routing_key
        << ", tag " << msg->delivery_tag
        << ", ex " << msg->exchange
        << ", content_type " << msg->content_type;
    if (msg->message.find("password") == string::npos) {
        log_msg << ", message " << msg->message;
    } else {
        #ifdef _DEBUG
            log_msg << ", (DEBUG) message " << msg->message;
        #else
            log_msg << " (message not printed since there was a password in it) ";
        #endif
    }
    NOVA_LOG_INFO(log_msg.str().c_str());
    JsonObjectPtr json_obj(new JsonObject(msg->message.c_str()));

    return boost::make_tuple(json_obj, msg->delivery_tag);
}

void Receiver::init_input_with_json(GuestInput & input, JsonObject & msg) {
    input.method_name = msg.get_string("method");
    input.tenant = msg.get_optional_string("_context_tenant");
    input.token = msg.get_optional_string("_context_auth_token");
    input.args = msg.get_object_or_empty("args");
}

GuestInput Receiver::next_message() {
    while(true) {
        JsonObjectPtr raw;
        int delivery_tag;
        try {
            boost::tie(raw, delivery_tag) = _next_message();
        } catch(const JsonException & je) {
            NOVA_LOG_ERROR("Message was not JSON! %s", je.what());
            throw GuestException(GuestException::MALFORMED_INPUT);
        }
        JsonObjectPtr msg;
        try {
            msg.reset(new JsonObject(raw->get_string("oslo.message")));
        } catch (const JsonException & je) {
            NOVA_LOG_ERROR("Oslo message could not be converted to dictionary.");
            NOVA_LOG_ERROR("%s", je.what());
            throw GuestException(GuestException::MALFORMED_INPUT);
        }
        optional<string> msg_id;
        try {
            msg_id = msg->get_string("_msg_id");
        } catch(const JsonException & je) {
            msg_id = boost::none;
        }
        msg_state.set_response_info(delivery_tag, msg_id);
        try {
            GuestInput input;
            init_input_with_json(input, *msg);
            return input;
        } catch(const JsonException & je) {
            NOVA_LOG_ERROR("Json message was malformed:", msg->to_string());
            throw GuestException(GuestException::MALFORMED_INPUT);
        }
    }
}


/**---------------------------------------------------------------------------
 *- ResilientReceiver
 *---------------------------------------------------------------------------*/

ResilientReceiver::ResilientReceiver(const char * host, int port,
    const char * userid, const char * password, size_t client_memory,
    const char * topic, const char * exchange_name,
    std::vector<unsigned long> reconnect_wait_times)
: ResilientConnection(host, port, userid, password, client_memory,
                      reconnect_wait_times),
  exchange_name(exchange_name),
  msg_state(),
  receiver(0),
  topic(topic)
{
    open(false);
}

ResilientReceiver::~ResilientReceiver() {
    close();
}

void ResilientReceiver::close() {
    msg_state = receiver->get_message_state();  // Restore on failure.
    receiver.reset(0);
}

void ResilientReceiver::finish_message(const GuestOutput & output) {
    while(true) {
        try {
            NOVA_LOG_INFO("Finishing message.");
            receiver->finish_message(output);
            return;
        } catch(const AmqpException & amqpe) {
            NOVA_LOG_ERROR("Error with AMQP connection! : %s", amqpe.what());
            reset();
        }
    }
}

bool ResilientReceiver::is_open() const {
    return receiver.get() != 0;
}

GuestInput ResilientReceiver::next_message() {
    while(true) {
        try {
            NOVA_LOG_INFO("Waiting for next message...");
            return receiver->next_message();
        } catch(const AmqpException & amqpe) {
            NOVA_LOG_ERROR("Error with AMQP connection! : %s", amqpe.what());
            reset();
        }
    }
}

void ResilientReceiver::finish_open(AmqpConnectionPtr connection) {
    receiver.reset(new Receiver(connection, topic.c_str(),
                                exchange_name.c_str(), msg_state));
}

} }  // end namespace
