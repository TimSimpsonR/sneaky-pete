#include "nova/guest/diagnostics.h"

#include "nova/guest/GuestException.h"
#include "nova_guest_version.hpp"
#include <boost/foreach.hpp>
#include "nova/Log.h"
#include <sstream>
#include <string>

using nova::JsonData;
using nova::JsonDataPtr;
using nova::Log;
using nova::guest::GuestException;
using boost::optional;
using std::string;
using std::stringstream;
using namespace boost;

namespace nova { namespace guest { namespace diagnostics {

namespace {
    string diagnostics_to_stream(DiagInfoPtr diagnostics) {
        stringstream out;
        out << "{";
        out << JsonData::json_string("version") << ": ";
        out << JsonData::json_string(diagnostics->version);
        out << ",";
        out << JsonData::json_string("fd_size") << ": " << diagnostics->fd_size;
        out << ",";
        out << JsonData::json_string("vm_size") << ": " << diagnostics->vm_size;
        out << ",";
        out << JsonData::json_string("vm_peak") << ": " << diagnostics->vm_peak;
        out << ",";
        out << JsonData::json_string("vm_rss") << ": " << diagnostics->vm_rss;
        out << ",";
        out << JsonData::json_string("vm_hwm") << ": " << diagnostics->vm_hwm;
        out << ",";
        out << JsonData::json_string("threads") << ": " << diagnostics->threads;
        out << "}";
        return out.str();
    }
} // end of anonymous namespace

InterrogatorMessageHandler::InterrogatorMessageHandler(const Interrogator & interrogator)
: interrogator(interrogator) {
}

JsonDataPtr InterrogatorMessageHandler::handle_message(const GuestInput & input) {
    NOVA_LOG_DEBUG("entering the handle_message method now ");
    if (input.method_name == "get_diagnostics") {
        NOVA_LOG_DEBUG("handling the get_diagnostics method");
        DiagInfoPtr diagnostics = interrogator.get_diagnostics();
        NOVA_LOG_DEBUG("returned from the get_diagnostics");
        if (diagnostics.get() != 0) {
            //convert from map to string
            string output = diagnostics_to_stream(diagnostics);
            NOVA_LOG_DEBUG2("output = %s", output.c_str());
            JsonDataPtr rtn(new JsonObject(output.c_str()));
            return rtn;
        } else {
            return JsonData::from_null();
        }
    } else {
        return JsonDataPtr();
    }
}



} } } // end namespace nova::guest::diagnostics
