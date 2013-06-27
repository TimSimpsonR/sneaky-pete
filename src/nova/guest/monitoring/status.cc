#include "nova/guest/monitoring/status.h"
#include <fstream>
#include <string>
#include <boost/format.hpp>
#include "nova/utils/io.h"
#include <sys/stat.h>
#include "nova/Log.h"
#include "nova/guest/apt.h"

using nova::guest::apt::AptGuest;
using namespace boost;
using nova::utils::io::is_directory;
using nova::utils::io::is_file;
using namespace std;


namespace nova { namespace guest { namespace monitoring { namespace status {

namespace {
    const char * MONITORING_PID_FILE = "/var/run/rackspace-monitoring-agent.pid";
    const double TIME_OUT = 500;
    const char * PACKAGE_NAME = "rackspace-monitoring-agent";
    const char * MONITORING_BIN_PATH = "/usr/bin/rackspace-monitoring-agent";
}

// Monitoring Status
MonitoringStatus::MonitoringStatus() {

}

boost::optional<boost::tuple<std::string, MonitoringStatus::Status> >
MonitoringStatus::get_monitoring_status(AptGuest & apt) {
    auto version = apt.version(PACKAGE_NAME, TIME_OUT);
    if (!version) {
        return boost::none;
    } else {
        auto status = get_current_status();
        return boost::make_tuple(version.get(), status);
    }
}

MonitoringStatus::Status MonitoringStatus::get_current_status() {
    NOVA_LOG_INFO2("opening the pid file (%s)", MONITORING_PID_FILE);

    if (!is_file(MONITORING_BIN_PATH)) {
        return REMOVED;
    }

    ifstream pid_file;
    pid_file.open(MONITORING_PID_FILE);
    string monitoring_agent_pid;
    // read the pid from pid_file
    if (! (pid_file.is_open() && pid_file.good())) {
        // if file does not exist set status to SHUTDOWN
        NOVA_LOG_INFO("Did not get a monitoring pid setting status to: SHUTDOWN");
        return SHUTDOWN;
    }
    pid_file >> monitoring_agent_pid;
    pid_file.close();

    // check if there was a pid
    if (monitoring_agent_pid.length() == 0){
        NOVA_LOG_INFO("Got a monitoring pid (empty) setting status to SHUTDOWN");
        return SHUTDOWN;
    }
    NOVA_LOG_INFO2("Got a monitoring pid of (%s)", monitoring_agent_pid.c_str());

    // it does? now does /proc/{pid} folder exist?
    string monitoring_agent_pid_path = "/proc/" + monitoring_agent_pid;
    if (is_directory(monitoring_agent_pid_path.c_str())) {
        return ACTIVE;
    } else {
        return SHUTDOWN;
    }
}


const char * MonitoringStatus::status_name(MonitoringStatus::Status status) {
    // Make sure this matches nova.compute.power_state!
    switch(status) {
        case ACTIVE:
            return "ACTIVE";
        case REMOVED:
            return "REMOVED";
        case SHUTDOWN:
            return "SHUTDOWN";
        // this is not used yet. should it be?
        case BUILDING:
            return "BUILDING";
        case UNKNOWN:
            return "UNKNOWN";
        default:
            return "!invalid status code!";
    }
}

} } } } // end namespace
