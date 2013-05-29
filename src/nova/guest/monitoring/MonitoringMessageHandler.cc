#include "nova/guest/monitoring/monitoring.h"
#include "nova/guest/monitoring/status.h"
#include "nova/guest/GuestException.h"
#include "nova/guest/guest.h"
#include "nova/guest/apt.h"
#include "nova_guest_version.hpp"
#include <boost/foreach.hpp>
#include "nova/Log.h"
#include <sstream>
#include <string>

using nova::JsonData;
using nova::JsonDataPtr;
using nova::Log;
using nova::JsonData;
using nova::JsonDataPtr;
using nova::JsonObject;
using nova::guest::GuestException;
using nova::guest::monitoring::status::MonitoringInfoPtr;
using nova::guest::monitoring::status::MonitoringStatus;
using nova::guest::apt::AptGuest;
using boost::optional;
using std::string;
using std::stringstream;
using namespace boost;

namespace nova { namespace guest { namespace monitoring {

namespace {

    const double time_out = 500;
    const bool use_sudo = true;
    const bool use_purge = true;
    // This is not used by Apt but a string is needed to create the object.
    const char * package = "rackspace-monitoring-agent";

    string monitoring_status_to_string(const MonitoringInfoPtr mon_ptr) {
        stringstream out;
        out << "{";
        out << JsonData::json_string("version") << ": \"" << mon_ptr->version.get_value_or("none") << "\"";
        out << ",";
        out << JsonData::json_string("status") << ": \"" << mon_ptr->status.get_value_or("none") << "\"";
        out << "}";
        return out.str();
    }

} // end of anonymous namespace

MonitoringMessageHandler::MonitoringMessageHandler(Monitoring & monitoring)
: monitoring(monitoring) {
}

JsonDataPtr MonitoringMessageHandler::handle_message(const GuestInput & input) {
    NOVA_LOG_DEBUG("entering the Monitoring handle_message method now ");
    if (input.method_name == "install_monitoring_agent") {
        NOVA_LOG_DEBUG("handling the install_monitoring_agent method");
        const auto monitoring_token = input.args->get_string("monitoring_token");
        const auto monitoring_endpoints = input.args->get_string("monitoring_endpoints");
        AptGuest apt(use_sudo, package, time_out, use_purge);
        monitoring.install_and_configure_monitoring_agent(apt, monitoring_token, monitoring_endpoints);
        return JsonData::from_null();
    } else if (input.method_name == "remove_monitoring_agent") {
        NOVA_LOG_DEBUG("handling the remove_monitoring_agent method");
        AptGuest apt(use_sudo, package, time_out, use_purge);
        monitoring.remove_monitoring_agent(apt);
        return JsonData::from_null();
    } else if (input.method_name == "start_monitoring_agent") {
        NOVA_LOG_DEBUG("handling the start_monitoring_agent method");
        monitoring.start_monitoring_agent();
        return JsonData::from_null();
    } else if (input.method_name == "stop_monitoring_agent") {
        NOVA_LOG_DEBUG("handling the stop_monitoring_agent method");
        monitoring.stop_monitoring_agent();
        return JsonData::from_null();
    } else if (input.method_name == "restart_monitoring_agent") {
        NOVA_LOG_DEBUG("handling the restart_monitoring_agent method");
        monitoring.restart_monitoring_agent();
        return JsonData::from_null();
    } else if (input.method_name == "get_monitoring_status") {
        NOVA_LOG_DEBUG("handling the get_monitoring_status method");
        AptGuest apt(use_sudo, package, time_out, use_purge);
        MonitoringStatus mon_status;
        MonitoringInfoPtr mon_ptr = mon_status.get_monitoring_status(apt);
        NOVA_LOG_DEBUG("formating data from get_monitoring_status");
        if (mon_ptr.get() != 0) {
            string output = monitoring_status_to_string(mon_ptr);
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



} } } // end namespace nova::guest::monitoring
