#ifndef __NOVAGUEST_SENDER_H
#define __NOVAGUEST_SENDER_H

#include <AMQPcpp.h>
#include <json/json.h>

class Sender {
    public:
        Sender(const char * host_name, const char * exchange_name,
               const char * queue_name);

        ~Sender();

        void send(json_object * object);

        void send(const char * publish_string);

    private:
        //TODO(tim.simpson): What is the policy regarding ex and queue?
        //                   Do we need to free them somehow?
        AMQP amqp;
        AMQPExchange * ex;
        const std::string exchange_name;
        AMQPQueue * queue;
};

#endif
