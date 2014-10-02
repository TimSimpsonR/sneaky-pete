#include "pch.hpp"
#include "nova/json.h"
#include "nova/rpc/sender.h"
#include "nova/rpc/amqp.h"
#include <sstream>
#include "nova/utils/subsecond.h"

using boost::format;
using nova::json_obj;
using nova::JsonObjectBuilder;
using nova::JsonObjectPtr;
using nova::utils::subsecond::now;
using namespace nova::rpc;
using std::string;


Sender::Sender(AmqpConnectionPtr connection, const char * topic)
:   exchange(),
    exchange_name("nova"),
    queue_name(topic),
    routing_key(topic)
{
    exchange = connection->new_channel();

    NOVA_LOG_DEBUG("Declaring sender exchange and queue.");
    connection->attempt_declare_exchange(exchange_name.c_str(), "direct");
    connection->attempt_declare_queue(queue_name.c_str(), false, true);

    NOVA_LOG_DEBUG("Binding queue to exchange.");
    connection->new_channel()->
        bind_queue_to_exchange(queue_name.c_str(), exchange_name.c_str(),
                               routing_key.c_str());
    NOVA_LOG_DEBUG("Creating exchange channel.");
    exchange = connection->new_channel();
}

Sender::~Sender() {
}

void Sender::send(const char * publish_string) {
    exchange->publish(exchange_name.c_str(), routing_key.c_str(),
                      publish_string);
}

void Sender::send(const JsonObject & publish_object) {
    NOVA_LOG_INFO("Sending message: %s", publish_object.to_string());
    send(publish_object.to_string());
}

ResilientSender::ResilientSender(const char * host, int port,
    const char * userid, const char * password, size_t client_memory,
    const char * topic, const char * exchange_name,
    const char * instance_id,
    const std::vector<unsigned long> reconnect_wait_times)
:   ResilientConnection(host, port, userid, password, client_memory,
                        reconnect_wait_times),
    exchange_name(exchange_name),
    instance_id(instance_id),
    sender(0),
    topic(topic),
    conductor_mutex()
{
    boost::lock_guard<boost::mutex> lock(conductor_mutex);
    open(false);
}

ResilientSender::~ResilientSender() {
    close();
}

void ResilientSender::close() {
    sender.reset(0);
}

void ResilientSender::finish_open(AmqpConnectionPtr connection) {
    sender.reset(new Sender(connection, topic.c_str()));
}

bool ResilientSender::is_open() const {
    return sender.get() != 0;
}

void ResilientSender::send(const char * method, JsonObjectBuilder & args) {
    args.add("instance_id", instance_id);
    args.add_unescaped("sent", str(format("%8.8f") % now()));
    std::string msg = boost::lexical_cast<std::string>(json_obj(
        "method", method,
        "args", args
    ));
    send_plain_string(msg.c_str());
}

void ResilientSender::send_plain_string(const char * msg) {
    NOVA_LOG_INFO("Sending message ]%s[", msg);
    boost::lock_guard<boost::mutex> lock(conductor_mutex);
    while(true)
    {
        try {
            sender->send(msg);
            return;
        } catch(const AmqpException & amqpe) {
            NOVA_LOG_ERROR("Error with AMQP connection! : %s", amqpe.what());
            reset();
        }
    }
}
