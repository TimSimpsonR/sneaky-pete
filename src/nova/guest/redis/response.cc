#include "pch.hpp"
#include "response.h"
#include <string>
#include "constants.h"
namespace nova { namespace redis {


Response::Response(std::string _status)
:   status(_status)
{
}


}}//end nova::redis namespace.
