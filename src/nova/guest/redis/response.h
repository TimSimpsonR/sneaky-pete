#ifndef RESPONSE_H
#define RESPONSE_H

#include <string>


namespace nova { namespace redis {


struct Response
/* This struct is used to represent responses from the redis server.
    * It returns the status. A predefined set of responses can be found in
    * redis_constants.h.
    * A breif description of the response and the data if any the response
    * contained.
    */
{
    std::string status;

    Response(std::string _status);

};


}}//end nova::redis namespace.
#endif /* RESPONSE_H */
