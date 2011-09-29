#include "nova/guest/apt.h"

#include "nova/log.h"
#include <sstream>
#include <string>

using nova::Log;
using nova::JsonArray;
using nova::JsonArrayPtr;
using nova::JsonObject;
using nova::JsonObjectPtr;
using std::string;
using std::stringstream;

namespace nova { namespace guest {

AptMessageHandler::AptMessageHandler() {
}

JsonObjectPtr AptMessageHandler::handle_message(JsonObjectPtr input) {
    string method_name;
    input->get_string("method", method_name);
    JsonObjectPtr args = input->get_object("args");

    std::stringstream rtn;

    try {
        if (method_name == "install") {
            apt::install(args->get_string("package_name"),
                         args->get_int("time_out"));
            rtn << "{}";
        } else if (method_name == "remove") {
            apt::remove(args->get_string("package_name"),
                        args->get_int("time_out"));
            rtn << "{}";
        } else if (method_name == "version") {
            string version = apt::version(args->get_string("package_name"));
            rtn << "{'version':'" << version << "'}";
        } else {
            JsonObjectPtr rtn;
            return rtn;
        }
    } catch(const AptException & ae) {
        rtn << "{ 'error':'" << ae.what() << "' }";
    }

    JsonObjectPtr rtn_obj(new JsonObject(rtn.str().c_str()));
    return rtn_obj;
}

} }  // end namespace nova::guest
