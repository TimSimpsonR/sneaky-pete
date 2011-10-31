#include "nova/guest/apt.h"

#include "nova/guest/GuestException.h"
#include "nova/Log.h"
#include <boost/optional.hpp>
#include <sstream>
#include <string>

using nova::JsonData;
using nova::JsonDataPtr;
using nova::Log;
using nova::guest::GuestException;
using boost::optional;
using std::string;
using std::stringstream;

namespace nova { namespace guest { namespace apt {

AptMessageHandler::AptMessageHandler() {
}

JsonDataPtr AptMessageHandler::handle_message(JsonObjectPtr input) {
    string method_name;
    input->get_string("method", method_name);
    JsonObjectPtr args = input->get_object("args");
    if (method_name == "install") {
        apt::install(args->get_string("package_name"),
                     args->get_int("time_out"));
        return JsonData::from_null();
    } else if (method_name == "remove") {
        apt::remove(args->get_string("package_name"),
                    args->get_int("time_out"));
        return JsonData::from_null();
    } else if (method_name == "version") {
        const char * package_name = args->get_string("package_name");
        optional<string> version = apt::version(package_name);
        if (version) {
            return JsonData::from_string(version.get().c_str());
        } else {
            return JsonData::from_null();
        }
    } else {
        return JsonDataPtr();
    }
}

} } } // end namespace nova::guest
