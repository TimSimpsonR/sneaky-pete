#include "nova/guest/diagnostics.h"

#include "nova/guest/GuestException.h"
#include "nova/guest/version.h"
#include <boost/foreach.hpp>
#include "nova/Log.h"
#include <sstream>
#include <string>

using nova::JsonData;
using nova::JsonDataPtr;
using nova::Log;
using nova::guest::CURRENT_VERSION;
using nova::guest::GuestException;
using boost::optional;
using std::string;
using std::stringstream;
using namespace boost;

namespace nova { namespace guest { namespace diagnostics {

namespace {
    void diagnostics_to_stream(stringstream & out, DiagInfoPtr diagnostics) {
        out << "{";
        out << "\"version\": " << 
                JsonData::json_string(CURRENT_VERSION);
        BOOST_FOREACH(DiagInfo::value_type &i, (*diagnostics)) {
            out << ",";
            out << JsonData::json_string(i.first) << ": " << i.second;
        }
        out << "}";
    }
} // end of anonymous namespace

InterrogatorMessageHandler::InterrogatorMessageHandler(const Interrogator & interrogator)
: interrogator(interrogator) {
}

JsonDataPtr InterrogatorMessageHandler::handle_message(const GuestInput & input) {
    NOVA_LOG_DEBUG("entering the handle_message method now ");
    if (input.method_name == "get_diagnostics") {
        NOVA_LOG_DEBUG("handling the get_diagnostics method");
        DiagInfoPtr diagnostics = interrogator.get_diagnostics(30);
                                            // input.args->get_int("time_out"));
        NOVA_LOG_DEBUG("returned from the get_diagnostics");
        if (diagnostics.get() != 0) {
            //convert from map to string
            stringstream out;
            diagnostics_to_stream(out, diagnostics);
            NOVA_LOG_DEBUG2("output = %s", out.str().c_str());
            JsonDataPtr rtn(new JsonObject(out.str().c_str()));
            return rtn;
        } else {
            return JsonData::from_null();
        }
    } else {
        return JsonDataPtr();
    }
}



} } } // end namespace nova::guest::diagnostics
