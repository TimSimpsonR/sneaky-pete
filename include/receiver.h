#ifndef __NOVAGUEST_RECEIVER_H
#define __NOVAGUEST_RECEIVER_H

#include "amqp_helpers_ptr.h"
#include <json/json.h>
#include "log.h"
#include <string>


class Receiver {

public:
    Receiver(const char * host, int port,
             const char * user_name, const char * password,
             const char * queue_name);

    ~Receiver();

    /** Finishes a message. */
    void finish_message(json_object * arguments, json_object * output);

    /** Grabs the next message. Caller is responsible for calling ack. */
    json_object * next_message();

private:
    AmqpConnectionPtr connection;
    int last_delivery_tag;
    Log log;
    AmqpChannelPtr queue;
    const std::string queue_name;

};

#endif
