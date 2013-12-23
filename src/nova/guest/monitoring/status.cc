#include "pch.hpp"
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


namespace nova { namespace guest { namespace monitoring {

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

optional<string> MonitoringStatus::read_pid_from_file() {
    ifstream pid_file;
    pid_file.open(MONITORING_PID_FILE);
    // read the pid from pid_file
    if (! (pid_file.is_open() && pid_file.good())) {
        return boost::none;
    }
    string monitoring_agent_pid;
    pid_file >> monitoring_agent_pid;
    if (monitoring_agent_pid.length() == 0){
        NOVA_LOG_INFO("Pid read from file was empty.");
        return boost::none;
    }
    return monitoring_agent_pid;
}

MonitoringStatus::Status MonitoringStatus::get_current_status() {
    NOVA_LOG_INFO("opening the pid file (%s)", MONITORING_PID_FILE);

    if (!is_file(MONITORING_BIN_PATH)) {
        return REMOVED;
    }

    auto optional_pid = read_pid_from_file();
    if (!optional_pid) {
        NOVA_LOG_INFO("Did not get a monitoring pid setting status to: SHUTDOWN");
        return SHUTDOWN;
    }
    auto pid = optional_pid.get();
    NOVA_LOG_INFO("Got a monitoring pid of (%s)", pid.c_str());

    // it does? now does /proc/{pid} folder exist?
    string proc_pid_path = "/proc/" + pid;
    if (is_directory(proc_pid_path.c_str())) {
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
        default:
            return "!invalid status code!";
    }
}

} } } // end namespace
