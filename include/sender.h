#ifndef __NOVAGUEST_SENDER_H
#define __NOVAGUEST_SENDER_H

#include "amqp_helpers_ptr.h"
#include <json/json.h>
#include "log.h"


class Sender {
    public:
        Sender(const char * host_name, int port,
               const char * user_name, const char * password,
               const char * exchange_name, const char * queue_name,
               const char * routing_key);

        ~Sender();

        void send(json_object * object);

        void send(const char * publish_string);

    private:
        AmqpChannelPtr exchange;
        const std::string exchange_name;
        Log log;
        AmqpChannelPtr queue;
        const std::string queue_name;
        const std::string routing_key;
};

#endif
