#ifndef __NOVA_RPC_AMQP_PTR_H
#define __NOVA_RPC_AMQP_PTR_H

#include <boost/smart_ptr.hpp>

namespace nova { namespace rpc {

    class AmqpConnection;
    class AmqpChannel;

    typedef boost::intrusive_ptr<AmqpConnection> AmqpConnectionPtr;
    typedef boost::intrusive_ptr<AmqpChannel> AmqpChannelPtr;

    void intrusive_ptr_add_ref(AmqpConnection * ref);
    void intrusive_ptr_release(AmqpConnection * ref);

    void intrusive_ptr_add_ref(AmqpChannel * ref);
    void intrusive_ptr_release(AmqpChannel * ref);

} }

#endif
