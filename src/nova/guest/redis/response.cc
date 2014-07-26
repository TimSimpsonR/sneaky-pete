#include "pch.hpp"
#include "response.h"
#include <string>
#include "constants.h"
namespace nova { namespace redis {


Response::Response(std::string _status,
                   std::string _data)
:   status(_status),
    data(_data)
{
}


}}//end nova::redis namespace.
