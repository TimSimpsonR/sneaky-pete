#include "guest.h"
#include "receiver.h"
#include <AMQPcpp.h>
#include <sstream>
#include <syslog.h>


Receiver::Receiver(const char * host, const char * queue_name,
                   const char * default_host)
:   amqp(host),
    current_message(0),
    default_host(default_host),
    host(host),
    queue_name(queue_name),
    temp_queue(0)
{
    temp_queue = amqp.createQueue(queue_name);
    //Assume the queue is already declared.
    temp_queue->Declare();
}

Receiver::~Receiver() {
}

void Receiver::finish_message(json_object * obj) {
    if (current_message == 0) {
        throw std::exception();
    }
    json_object_put(obj);
    temp_queue->Ack(current_message->getDeliveryTag());
}

json_object * Receiver::next_message() {
    if (current_message != 0) {
        temp_queue->Get();
    }
    while (current_message == 0 || current_message->getMessageCount() < 0) {
        temp_queue->Get();
        current_message = temp_queue->getMessage();
    }
    syslog(LOG_INFO, "message %s, key %s, tag %i, ex %s, c-type %s, c-enc %s",
                     current_message->getMessage(),
                     current_message->getRoutingKey().c_str(),
                     current_message->getDeliveryTag(),
                     current_message->getExchange().c_str(),
                     current_message->getHeader("Content-type").c_str(),
                     current_message->getHeader("Content-encoding").c_str());
    json_object *new_obj = json_tokener_parse(current_message->getMessage());
    return new_obj;
}
