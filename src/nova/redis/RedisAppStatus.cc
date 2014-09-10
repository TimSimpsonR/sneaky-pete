#include "pch.hpp"
#include "nova/redis/RedisAppStatus.h"

#include <boost/assign/list_of.hpp>
#include <boost/algorithm/string.hpp>
#include "nova/redis/RedisClient.h"
#include "nova/redis/RedisException.h"
#include "nova/Log.h"
#include "nova/datastores/DatastoreStatus.h"
#include <boost/optional.hpp>
#include "nova/process.h"
#include "nova/rpc/sender.h"
#include <boost/thread.hpp>
#include <string>

using namespace boost::assign;
using nova::datastores::DatastoreStatus;
using nova::process::execute;
using boost::optional;
using nova::rpc::ResilientSenderPtr;
using nova::Log;
using nova::process::is_pid_alive;
using std::string;
using std::stringstream;

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
    try {
        NOVA_LOG_INFO("Attempting to get redis-server status");
        RedisClient client;
        client.ping();
        return RUNNING;
    } catch(const RedisException & re) {
        NOVA_LOG_INFO("Error pinging Redis, getting pid instead.");
    }
    auto pid = get_pid();
    if (pid) {
        if (is_pid_alive(pid.get())) {
            NOVA_LOG_INFO("Redis is Blocked");
            return BLOCKED;
        } else {
            NOVA_LOG_INFO("Redis is Crashed");
            return CRASHED;
        }
    }  else {
        NOVA_LOG_INFO("Redis is shutdown");
        return SHUTDOWN;
    }
}

optional<int> RedisAppStatus::get_pid()
{
    try {
        stringstream output_stream;
        execute(output_stream, list_of("/bin/pidof")("redis-server")("r"));
        string output = output_stream.str();
        boost::trim(output);
        if (output.length() > 0) {
            const auto pid = boost::lexical_cast<int>(output);
            if (0 == pid) {
                return boost::none;
            } else {
                return pid;
            }
        }
    } catch(const process::ProcessException & pe) {
    } catch(const boost::bad_lexical_cast & blc) {
        NOVA_LOG_ERROR("Couldn't parse pid file.");
    }
    return boost::none;
}

}} // end nova::redis
