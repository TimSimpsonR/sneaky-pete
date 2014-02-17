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
    const char * instance_id, unsigned long reconnect_wait_time)
:   client_memory(client_memory),
    exchange_name(exchange_name),
    host(host),
    instance_id(instance_id),
    password(password),
    port(port),
    sender(0),
    topic(topic),
    userid(userid),
    reconnect_wait_time(reconnect_wait_time),
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

void ResilientSender::open(bool wait_first) {
    while(sender.get() == 0) {
        try {
            if(wait_first) {
                NOVA_LOG_INFO("Waiting to create fresh AMQP connection...");
                boost::posix_time::seconds time(reconnect_wait_time);
                boost::this_thread::sleep(time);
            }
            AmqpConnectionPtr connection =
                AmqpConnection::create(host.c_str(), port, userid.c_str(),
                    password.c_str(), client_memory);
            sender.reset(new Sender(connection, topic.c_str()));

            return;
        } catch(const AmqpException & amqpe) {
            NOVA_LOG_ERROR("Error establishing AMQP connection: %s",
                           amqpe.what());
            wait_first = true;
        }
    }
}

void ResilientSender::reset() {
    close();
    open(true);
}

void ResilientSender::finish_send(const char * method,
                                  JsonObjectBuilder & args) {
    std::string msg = boost::lexical_cast<std::string>(json_obj(
        "method", method,
        "args", args
    ));
    send_plain_string(msg.c_str());
}



void ResilientSender::start_send(const char * method, JsonObjectBuilder & args) {
    args.add("instance_id", instance_id);
    args.add_unescaped("sent", str(format("%8.8f") % now()));
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
