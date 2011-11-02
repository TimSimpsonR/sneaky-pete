#include "nova/json.h"
#include "nova/rpc/sender.h"
#include "nova/rpc/amqp.h"
#include <sstream>


using nova::JsonObjectPtr;
using namespace nova::rpc;


Sender::Sender(const char * host_name, int port,
               const char * user_name, const char * password,
               const char * exchange_name, const char * queue_name,
               const char * routing_key)
:   exchange(),
    exchange_name(exchange_name),
    log(),
    queue(),
    queue_name(queue_name),
    routing_key(routing_key)
{
    AmqpConnectionPtr connection = AmqpConnection::create(host_name, port,
                                                          user_name, password,
                                                          1024 * 4);
    exchange = connection->new_channel();
    //exchange->declare_exchange(exchange_name, "direct");

    queue = connection->new_channel();
    //queue->declare_queue(queue_name, false, true);
    //queue->bind_queue_to_exchange(queue_name, exchange_name, routing_key);
}

Sender::~Sender() {
}

void Sender::send(const char * publish_string) {
    exchange->publish(exchange_name.c_str(), routing_key.c_str(),
                      publish_string);
}


void Sender::send(JsonObjectPtr publish_object) {
    log.info2("Sending message: %s", publish_object->to_string());
    send(publish_object->to_string());
}
