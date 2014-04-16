#include "pch.hpp"
#include "nova/guest/redis/status.h"

#include "nova/guest/redis/client.h"
#include "nova/guest/redis/constants.h"
#include "nova/Log.h"
#include "nova/datastores/DatastoreStatus.h"
#include "nova/rpc/sender.h"
#include <boost/thread.hpp>
#include <string>

using nova::datastores::DatastoreStatus;
using nova::rpc::ResilientSenderPtr;
using nova::Log;

namespace nova { namespace redis {


RedisAppStatus::RedisAppStatus(ResilientSenderPtr sender,
                               bool is_redis_installed) :
                               DatastoreStatus(sender, is_redis_installed)
{
}

RedisAppStatus::~RedisAppStatus()
{
}

/*
 * Get the status of a redis instance
 * First we attempt to ping the redis instance
 * if all goes well we return running.
 *
 * Next we check to see if the process is still running
 * by SIGNULLing it. If that is true we return blocked.
 *
 * If the client's pid file still has a pid. We assume it crashed.
 * returning crashed
 *
 * Finally we return shutdown if none of the previous conditions are met.
 */
DatastoreStatus::Status RedisAppStatus::determine_actual_status() const
{
    // RUNNING = We could ping redis.
    // BLOCKED = We can't ping it, but we can see the process running.
    // CRASHED = The process is dead, but left evidence it once existed.
    // SHUTDOWN = The process is dead and never existed or cleaned itself up.
    nova::redis::Client client(SOCKET_NAME,
                               REDIS_PORT,
                               REDIS_AGENT_NAME,
                               DEFAULT_REDIS_CONFIG);
    NOVA_LOG_INFO("Attempting to get redis-server status");
    if(client.ping().status == STRING_RESPONSE)
    {
        NOVA_LOG_INFO("Redis is Running");
        return RUNNING;
    }
    else if(client.control->get_process_status() == 0)
    {
        NOVA_LOG_INFO("Redis is Blocked");
        return BLOCKED;
    }
    else if(client.control->get_pid() != -1)
    {
        NOVA_LOG_INFO("Redis is Crashed");
        return CRASHED;
    }
    else
    {
        NOVA_LOG_INFO("Redis is shutdown");
        return SHUTDOWN;
    }
}


}} // end nova::redis
