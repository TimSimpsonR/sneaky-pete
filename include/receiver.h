#ifndef __NOVAGUEST_RECEIVER_H
#define __NOVAGUEST_RECEIVER_H

#include <AMQPcpp.h>
#include <json/json.h>
#include <string>


class Receiver {

public:
    Receiver(const char * host, const char * queue_name,
             const char * default_host);

    ~Receiver();

    /** Finishes a message. */
    void finish_message(json_object * arguments, json_object * output);

    /** Grabs the next message. Caller is responsible for calling ack. */
    json_object * next_message();

private:
    AMQP amqp;
    AMQPMessage * current_message;
    const std::string default_host;
    const std::string host;
    const std::string queue_name;
    AMQPQueue * temp_queue;

};

#endif
