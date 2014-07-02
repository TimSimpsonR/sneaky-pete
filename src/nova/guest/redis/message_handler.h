#ifndef NOVA_REDIS_REDIS_MESSAGE_HANDLER_H
#define NOVA_REDIS_REDIS_MESSAGE_HANDLER_H

#include "nova/redis/RedisApp.h"
#include "nova/guest/guest.h"
#include "nova/guest/redis/status.h"
#include "nova/guest/common/PrepareHandler.h"

namespace nova { namespace guest {


class RedisMessageHandler : public MessageHandler {

    public:
        RedisMessageHandler(
            nova::redis::RedisAppPtr app,
            nova::guest::common::PrepareHandlerPtr prepare_handler,
            nova::redis::RedisAppStatusPtr status);

        virtual ~RedisMessageHandler();

        virtual nova::JsonDataPtr handle_message(const GuestInput & input);

    private:
        nova::redis::RedisAppPtr app;

        nova::redis::RedisAppStatusPtr app_status;

        nova::guest::common::PrepareHandlerPtr prepare_handler;

        RedisMessageHandler(const RedisMessageHandler &);

        RedisMessageHandler & operator = (const RedisMessageHandler &);

};


}}//end nova::redis namespace.
#endif //NOVA_REDIS_REDIS_MESSAGE_HANDLER_H
