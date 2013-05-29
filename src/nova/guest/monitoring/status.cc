#include "nova/guest/monitoring/status.h"
#include <fstream>
#include <string>
#include <boost/format.hpp>
#include <sys/stat.h>
#include "nova/Log.h"
#include "nova/guest/apt.h"

using nova::guest::apt::AptGuest;
using namespace boost;
using namespace std;


namespace nova { namespace guest { namespace monitoring { namespace status {

namespace {
    const char * MONITORING_PID_FILE = "/var/run/rackspace-monitoring-agent.pid";
    const double TIME_OUT = 500;
    const char * PACKAGE_NAME = "rackspace-monitoring-agent";
}

// Monitoring Status
MonitoringStatus::MonitoringStatus(): status(boost::none){

}

MonitoringInfoPtr MonitoringStatus::get_monitoring_status(AptGuest & apt){
    MonitoringInfoPtr mon_ptr(new MonitoringStatusInfo());

    mon_ptr->version = apt.version(PACKAGE_NAME, TIME_OUT);

    // get the monitoring status
    MonitoringStatus stats;
    mon_ptr->status = stats.get_current_status_string(mon_ptr->version);

    return mon_ptr;
}

const char * MonitoringStatus::get_current_status_string(optional<string> version) {
    if(!version) {
        status = REMOVED;
    }
    find_monitoring_agent_status();
    if (status) {
        return status_name(status.get());
    } else {
        return "none";
    }
}

void MonitoringStatus::find_monitoring_agent_status() {
    NOVA_LOG_INFO2("opening the pid file (%s)", MONITORING_PID_FILE);
    ifstream pid_file;
    pid_file.open(MONITORING_PID_FILE);
    string monitoring_agent_pid;
    // read the pid from pid_file
    if (pid_file.is_open()) {
        while (pid_file.good()) {
            pid_file >> monitoring_agent_pid;
            break;
        }
    }
    else {
        // if file does not exist set status to SHUTDOWN
        NOVA_LOG_INFO("Did not get a monitoring pid setting status to: SHUTDOWN");
        status = SHUTDOWN;
        return;
    }
    pid_file.close();

    // check if there was a pid
    if (monitoring_agent_pid.length() == 0){
        NOVA_LOG_INFO("Got a monitoring pid (empty) setting status to SHUTDOWN");
        status = SHUTDOWN;
        return;
    }
    NOVA_LOG_INFO2("Got a monitoring pid of (%s)", monitoring_agent_pid.c_str());

    // it does? now does /proc/{pid} folder exist?
    string monitoring_agent_pid_path = "/proc/"+monitoring_agent_pid;
    const char * path = monitoring_agent_pid_path.c_str();
    // C has multiple namespaces for structs and functions.
    struct stat file_info;
    if (0 != stat(path, &file_info)) {
        NOVA_LOG_INFO("Filesystem is_directory check failed");
    }
    if (S_ISDIR(file_info.st_mode)) {
        status = ACTIVE;
        NOVA_LOG_INFO("monitoring directory found setting status to: ACTIVE");
    } else {
        status = SHUTDOWN;
        NOVA_LOG_INFO("monitoring directory found setting status to: SHUTDOWN");
    }
}


const char * MonitoringStatus::status_name(MonitoringStatus::Status status) {
    // Make sure this matches nova.compute.power_state!
    switch(status) {
        case ACTIVE:
            return "active";
        case REMOVED:
            return "removed";
        case SHUTDOWN:
            return "shutdown";
        // this is not used yet. should it be?
        case BUILDING:
            return "building";
        case UNKNOWN:
            return "unknown_case";
        default:
            return "!invalid status code!";
    }
}

} } } } // end namespace
