#include "nova/rpc/sender.h"
#include "nova/rpc/amqp.h"
#include <sstream>


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
                                                          user_name, password);
    exchange = connection->new_channel();
    exchange->declare_exchange(exchange_name);

    queue = connection->new_channel();
    queue->declare_queue(queue_name);
    queue->bind_queue_to_exchange(queue_name, exchange_name, routing_key);
}

Sender::~Sender() {
}

void Sender::send(const char * publish_string) {
    exchange->publish(exchange_name.c_str(), routing_key.c_str(),
                      publish_string);
}


void Sender::send(json_object * publish_object) {
    const char * str = json_object_get_string(publish_object);
    std::stringstream msg;
    msg << "Sending message " << str;
    log.info(msg.str());
    send(str);
}
