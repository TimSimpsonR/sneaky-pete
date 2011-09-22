#ifndef __NOVAGUEST_AMQP_HEADERS_PTR
#define __NOVAGUEST_AMQP_HEADERS_PTR

#include <boost/smart_ptr.hpp>

class AmqpConnection;
class AmqpChannel;

typedef boost::intrusive_ptr<AmqpConnection> AmqpConnectionPtr;
typedef boost::intrusive_ptr<AmqpChannel> AmqpChannelPtr;

void intrusive_ptr_add_ref(AmqpConnection * ref);
void intrusive_ptr_release(AmqpConnection * ref);

void intrusive_ptr_add_ref(AmqpChannel * ref);
void intrusive_ptr_release(AmqpChannel * ref);

#endif
