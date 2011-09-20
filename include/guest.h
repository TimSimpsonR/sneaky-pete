#ifndef __NOVAGUEST_GUEST_H
#define __NOVAGUEST_GUEST_H

#include <cstdlib>  // Needed for json/json.h below... :(
#include <json/json.h>
#include <boost/smart_ptr.hpp>

class MessageHandler {

public:

    /** Takes a JSON object as input and returns one as output. Returns nullptr
     *  if it doesn't know how to handle the input. */
    virtual json_object * handle_message(json_object * json) = 0;

};

typedef boost::shared_ptr<MessageHandler> MessageHandlerPtr;

#endif //__NOVAGUEST_GUEST_H
