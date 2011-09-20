#include "sender.h"


Sender::Sender(const char * host_name, const char * exchange_name,
               const char * queue_name)
:   amqp(host_name),
    ex(0),
    exchange_name(exchange_name),
    queue(0)
{
    ex = amqp.createExchange(exchange_name);
    ex->Declare(exchange_name, "direct");
    //Make sure the exchange was bound to the queue to actually send the messages thru
    queue = amqp.createQueue(queue_name);
    queue->Declare();
    queue->Bind(exchange_name, "");
}

Sender::~Sender() {
    delete queue;
    delete ex;
}

void Sender::send(const char * publish_string) {
    ex->setHeader("Delivery-mode", 2);
    ex->setHeader("Content-type", "text/text");
    ex->setHeader("Content-encoding", "UTF-8");
    ex->Publish(publish_string, "");
}

void Sender::send(json_object * publish_object) {
    const char * str = json_object_to_json_string(publish_object);
    send(str);
}
