#include "nova/json.h"
#include "nova/rpc/sender.h"
#include "nova/rpc/amqp.h"
#include <sstream>


using nova::JsonObjectPtr;
using namespace nova::rpc;


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
    NOVA_LOG_INFO2("Sending message: %s", publish_object.to_string());
    send(publish_object.to_string());
}
