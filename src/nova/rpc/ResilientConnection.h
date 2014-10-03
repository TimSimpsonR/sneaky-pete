#ifndef __NOVA_RPC_RESILIENTCONNECTION_H
#define __NOVA_RPC_RESILIENTCONNECTION_H

#include "nova/rpc/amqp_ptr.h"
#include <json/json.h>
#include "nova/guest/guest.h"
#include "nova/Log.h"
#include <memory>
#include <boost/optional.hpp>
#include <string>
#include <boost/utility.hpp>
#include <boost/smart_ptr.hpp>

namespace nova { namespace rpc {

    class ResilientConnection {
        public:
            ResilientConnection(const char * host, int port, const char * userid,
                            const char * password, size_t client_memory,
                         const std::vector<unsigned long> reconnect_wait_times);

            virtual ~ResilientConnection();

        protected:

            // Closes whatever is represented by this connection.
            virtual void close() = 0;

            virtual void finish_open(AmqpConnectionPtr connection) = 0;

            // True when whatever represented by this connection is open.
            virtual bool is_open() const = 0;

            // Re-opens whatever is represented by this connection.
            void open(bool wait_first);

            void reset();

        private:
            ResilientConnection(const ResilientConnection &);
            ResilientConnection & operator = (const ResilientConnection &);

            size_t client_memory;

            std::string host;

            std::string password;

            int port;

            unsigned int reconnect_wait_time_index;

            std::vector<unsigned long> reconnect_wait_times;

            std::string userid;
    };

} }  // end namespace

#endif
