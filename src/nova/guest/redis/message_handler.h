#ifndef NOVA_REDIS_REDIS_MESSAGE_HANDLER_H
#define NOVA_REDIS_REDIS_MESSAGE_HANDLER_H

#include "nova/guest/guest.h"
#include "nova/guest/redis/status.h"
#include "nova/guest/monitoring/monitoring.h"

namespace nova { namespace guest {


class RedisMessageHandler : public MessageHandler {

    public:

        RedisMessageHandler(
            nova::guest::apt::AptGuestPtr apt,
            nova::redis::RedisAppStatusPtr,
            nova::guest::monitoring::MonitoringManagerPtr monitoring);

        virtual ~RedisMessageHandler();

        virtual nova::JsonDataPtr handle_message(const GuestInput & input);

    private:
        nova::guest::apt::AptGuestPtr apt;

        nova::redis::RedisAppStatusPtr app_status;

        nova::guest::monitoring::MonitoringManagerPtr monitoring;

        RedisMessageHandler(const RedisMessageHandler &);

        RedisMessageHandler & operator = (const RedisMessageHandler &);

};


}}//end nova::redis namespace.
#endif //NOVA_REDIS_REDIS_MESSAGE_HANDLER_H
