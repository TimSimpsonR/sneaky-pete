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
using nova::JsonObject;
using nova::JsonObjectPtr;
using nova::Log;
using std::string;

namespace nova { namespace rpc {

namespace {
    const char * EMPTY_MESSAGE = "{ \"failure\": null, \"result\":null }";
}


/**---------------------------------------------------------------------------
 *- Receiver
 *---------------------------------------------------------------------------*/

Receiver::Receiver(AmqpConnectionPtr connection, const char * topic,
                   const char * exchange_name)
:   connection(connection),
    last_delivery_tag(-1),
    last_msg_id(boost::none),
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

void Receiver::finish_message(const GuestOutput & output) {
    queue->ack_message(last_delivery_tag);

    if (!last_msg_id) {
        // No reply necessary.
        NOVA_LOG_INFO("Acknowledged message but will not send reply because "
                      "no _msg_id was given.");
        return;
    }

    // Send reply.
    string exchange_name_str = str(format("__agent_response_%s")
                                   % last_msg_id.get());

    //const char * const queue_name = last_msg_id.c_str();
    const char * const exchange_name = last_msg_id.get().c_str();
    const char * const routing_key = last_msg_id.get().c_str(); //"";
    // queue_name, exchange_name, and routing_key are all the same.
    AmqpChannelPtr rtn_ex_channel = connection->new_channel();
    string msg;
    if (!output.failure) {
        msg = str(format("{ \"failure\":null, \"result\":%s }")
                  % output.result->to_string());
        //msg = "{ 'failure': null }";
        //msg = "{\"failure\":null}";
        JsonObject obj(msg.c_str());
        msg = obj.to_string();
    } else {
        msg = str(format("{\"failure\":{\"exc_type\":\"std::exception\", "
                         "\"value\":%s, "
                         "\"traceback\":\"unavailable\" } }")
                  % JsonData::json_string(output.failure.get().c_str()));
    }
    if (msg.find("password") == string::npos) {
        NOVA_LOG_INFO2("Replying with the following: %s", msg.c_str());
    } else {
        NOVA_LOG_INFO("Replying to message...");
        #ifdef _DEBUG
            NOVA_LOG_INFO2("(DEBUG) Replying with the following: %s",
                           msg.c_str());
        #endif
    }

    rtn_ex_channel->publish(exchange_name, routing_key, msg.c_str());

    // This is like telling Nova "roger."
    NOVA_LOG_INFO2("Replying with empty message: %s", EMPTY_MESSAGE);
    rtn_ex_channel->publish(exchange_name, routing_key, EMPTY_MESSAGE);
}

JsonObjectPtr Receiver::_next_message() {
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

    last_delivery_tag = msg->delivery_tag;
    return json_obj;
}

GuestInput Receiver::next_message() {
    while(true) {
        JsonObjectPtr raw;
        try {
            raw = _next_message();
        } catch(const JsonException & je) {
            NOVA_LOG_ERROR2("Message was not JSON! %s", je.what());
            throw GuestException(GuestException::MALFORMED_INPUT);
        }
        try {
            last_msg_id = raw->get_string("_msg_id");
        } catch(const JsonException & je) {
            last_msg_id = boost::none;
        }
        try {

            GuestInput input;
            input.method_name = raw->get_string("method");
            input.args = raw->get_object_or_empty("args");
            return input;
        } catch(const JsonException & je) {
            NOVA_LOG_ERROR2("Json message was malformed:", raw->to_string());
            throw GuestException(GuestException::MALFORMED_INPUT);
        }
    }
}


/**---------------------------------------------------------------------------
 *- ResilentReceiver
 *---------------------------------------------------------------------------*/

ResilentReceiver::ResilentReceiver(const char * host, int port,
    const char * userid, const char * password, size_t client_memory,
    const char * topic, const char * exchange_name,
    unsigned long reconnect_wait_time)
: client_memory(client_memory),
  exchange_name(exchange_name),
  host(host),
  password(password),
  port(port),
  receiver(0),
  topic(topic),
  userid(userid),
  reconnect_wait_time(reconnect_wait_time)
{
    open(false);
}

ResilentReceiver::~ResilentReceiver() {
    close();
}

void ResilentReceiver::close() {
    receiver.reset(0);
}

void ResilentReceiver::finish_message(const GuestOutput & output) {
    while(true) {
        try {
            NOVA_LOG_INFO("Finishing message.");
            receiver->finish_message(output);
            return;
        } catch(const AmqpException & amqpe) {
            NOVA_LOG_ERROR2("Error with AMQP connection! : %s", amqpe.what());
            reset();
        }
    }
}

GuestInput ResilentReceiver::next_message() {
    while(true) {
        try {
            NOVA_LOG_INFO("Waiting for next message...");
            return receiver->next_message();
        } catch(const AmqpException & amqpe) {
            NOVA_LOG_ERROR2("Error with AMQP connection! : %s", amqpe.what());
            reset();
        }
    }
}

void ResilentReceiver::open(bool wait_first) {
    while(receiver.get() == 0) {
        try {
            if (wait_first) {
                NOVA_LOG_INFO("Waiting to create fresh AMQP connection...");
                boost::posix_time::seconds time(reconnect_wait_time);
                boost::this_thread::sleep(time);
            }
            AmqpConnectionPtr connection =
                AmqpConnection::create(host.c_str(), port, userid.c_str(),
                    password.c_str(), client_memory);
            receiver.reset(new Receiver(connection, topic.c_str(),
                                        exchange_name.c_str()));
            return;
        } catch(const AmqpException & amqpe) {
            NOVA_LOG_ERROR2("Error establishing AMQP connection: %s",
                            amqpe.what());
            wait_first = true;
        }
    }
}

void ResilentReceiver::reset() {
    close();
    open(true);
}

} }  // end namespace
