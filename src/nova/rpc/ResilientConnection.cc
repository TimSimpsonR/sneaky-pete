#include "pch.hpp"
#include "ResilientConnection.h"
#include "nova/rpc/amqp.h"

using namespace nova::rpc;
using std::string;

ResilientConnection::ResilientConnection(const char * host, int port,
    const char * userid, const char * password, size_t client_memory,
    const std::vector<unsigned long> reconnect_wait_times)
:   client_memory(client_memory),
    host(host),
    password(password),
    port(port),
    reconnect_wait_time_index(0),
    reconnect_wait_times(reconnect_wait_times),
    userid(userid)
{
}

ResilientConnection::~ResilientConnection() {
}

void ResilientConnection::open(bool wait_first) {
    reconnect_wait_time_index = 0;
    while(!is_open()) {
        try {
            if(wait_first) {
                const auto seconds =
                    reconnect_wait_times[reconnect_wait_time_index];
                NOVA_LOG_INFO("Waiting to %d seconds to create a fresh AMQP "
                              "connection...", seconds);
                boost::posix_time::seconds time(seconds);
                boost::this_thread::sleep(time);
            }
            AmqpConnectionPtr connection =
                AmqpConnection::create(host.c_str(), port, userid.c_str(),
                    password.c_str(), client_memory);
            finish_open(connection);
            return;
        } catch(const AmqpException & amqpe) {
            NOVA_LOG_ERROR("Error establishing AMQP connection: %s",
                           amqpe.what());
            wait_first = true;
            if (reconnect_wait_time_index + 1 < reconnect_wait_times.size()) {
                ++ reconnect_wait_time_index;
            }
        }
    }
}

void ResilientConnection::reset() {
    close();
    open(true);
}
