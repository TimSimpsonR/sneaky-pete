#ifndef __NOVA_GUEST_GUEST_H
#define __NOVA_GUEST_GUEST_H

#include <cstdlib>  // Needed for json/json.h below... :(
#include <nova/json.h>
#include <boost/smart_ptr.hpp>


namespace nova { namespace guest {

    class MessageHandler {

    public:

        /** Takes a JSON object as input and returns one as output. Returns nullptr
         *  if it doesn't know how to handle the input. */
        virtual nova::JsonObjectPtr handle_message(JsonObjectPtr json) = 0;

    };

    typedef boost::shared_ptr<MessageHandler> MessageHandlerPtr;

} }

#endif //__NOVA_GUEST_GUEST_H
