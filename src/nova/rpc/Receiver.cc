#include "nova/rpc/receiver.h"
#include "nova/rpc/amqp.h"

#include <boost/format.hpp>
#include "nova/Log.h"
#include <string>
#include <sstream>

using boost::format;
using nova::JsonObject;
using nova::JsonObjectPtr;
using nova::Log;
using std::string;

namespace nova { namespace rpc {


Receiver::Receiver(const char * host, int port,
                   const char * user_name, const char * password,
                   const char * topic, size_t client_memory)
:   connection(),
    last_delivery_tag(-1),
    log(),
    queue(),
    topic(topic)
{
    connection = AmqpConnection::create(host, port, user_name, password,
                                        client_memory);
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
    const char * exchange_name = "nova";

    //MB queue->declare_queue(queue_name);


    queue->declare_queue(queue_name);

    // This should be legit but if both exchanges are declared it doesn't work.
    // TODO(tim.simpson): Move this code somewhere else, maybe its own channel,
    // to make sure the exchange at least exists...
    // try {
    //     queue->declare_exchange(exchange_name, "direct");
    // } catch(const AmqpException & ae) {
    //     if (ae.code == AmqpException::EXCHANGE_DECLARE_FAIL) {
    //         log.info2("Could not declare exchange %s. Was it already declared?",
    //                   exchange_name);
    //     } else {
    //         log.error("AmqpException was thrown trying to declare exchange.");
    //         throw ae;
    //     }
    // }
    queue->declare_exchange(topic, "direct");
    queue->bind_queue_to_exchange(queue_name, exchange_name, queue_name);
}

Receiver::~Receiver() {
}

void Receiver::finish_message(JsonObjectPtr input, JsonObjectPtr output) {
    queue->ack_message(last_delivery_tag);

    // Send reply.
    string msg_id_str;
    try {
        msg_id_str = input->get_string("_msg_id");
    } catch(const JsonException & je) {
        // If _msg_id wasn't found, ignore it.
        log.info("JsonException sending response.");
        return;
    }
    string exchange_name_str = str(format("__agent_response_%s") % msg_id_str);

    //const char * const queue_name = msg_id_str.c_str();
    const char * const exchange_name = msg_id_str.c_str();
    const char * const routing_key = msg_id_str.c_str(); //"";

    // queue_name, exchange_name, and routing_key are all the same.
    AmqpChannelPtr rtn_ex_channel = connection->new_channel();
    //rtn_ex_channel->declare_exchange(exchange_name, "direct");

    //AmqpChannelPtr rtn_queue_channel = connection->new_channel();
    //rtn_queue_channel->declare_queue(queue_name);
    //rtn_queue_channel->bind_queue_to_exchange(queue_name, exchange_name,
    //                                          routing_key);
    //rtn_ex_channel->bind_queue_to_exchange(queue_name, exchange_name,
    //                                          routing_key);
    Log log;
    log.info2("Outputting the following: %s", output->to_string());
    rtn_ex_channel->publish(exchange_name, routing_key, output->to_string());
}

JsonObjectPtr Receiver::next_message() {
    AmqpQueueMessagePtr msg;
    while(!msg) {
        msg = queue->get_message(topic.c_str());
        if (!msg) {
            log.info("Received an empty message.");
        }
    }
    std::stringstream log_msg;
    log_msg << "Received message "
        << ", key " << msg->routing_key
        << ", tag " << msg->delivery_tag
        << ", ex " << msg->exchange
        << ", content_type " << msg->content_type
        << ", message " << msg->message;
    log.info(log_msg.str());
    JsonObjectPtr new_obj(new JsonObject(msg->message.c_str()));

    last_delivery_tag = msg->delivery_tag;
    return new_obj;
}

} }  // end namespace
