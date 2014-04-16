#ifndef __NOVA_REDIS_APP_STATUS_H
#define __NOVA_REDIS_APP_STATUS_H


#include <list>
#include "nova/datastores/DatastoreStatus.h"
#include "nova/rpc/sender.h"
#include <boost/optional.hpp>
#include <string>


namespace nova { namespace redis {

class RedisAppStatus : public nova::datastores::DatastoreStatus {
    public:
        RedisAppStatus(nova::rpc::ResilientSenderPtr sender,
                       bool is_redis_installed);

        virtual ~RedisAppStatus();

    protected:
        virtual Status determine_actual_status() const;

};

typedef boost::shared_ptr<RedisAppStatus> RedisAppStatusPtr;


}}//end nova::redis namespace


#endif
