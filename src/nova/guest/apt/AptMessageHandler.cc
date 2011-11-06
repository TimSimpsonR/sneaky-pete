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

AptMessageHandler::AptMessageHandler(AptGuest * apt_guest)
: apt_guest(apt_guest) {
}

JsonDataPtr AptMessageHandler::handle_message(const std::string & method_name,
                                              JsonObjectPtr args) {
    if (method_name == "install") {
        apt_guest->install(args->get_string("package_name"),
                           args->get_int("time_out"));
        return JsonData::from_null();
    } else if (method_name == "remove") {
        apt_guest->remove(args->get_string("package_name"),
                                args->get_int("time_out"));
        return JsonData::from_null();
    } else if (method_name == "version") {
        const char * package_name = args->get_string("package_name");
        optional<string> version = apt_guest->version(package_name);
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
