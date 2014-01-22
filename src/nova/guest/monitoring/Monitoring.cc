#include "pch.hpp"
#include "nova/guest/monitoring/monitoring.h"
#include <boost/format.hpp>
#include <fstream>
#include <iostream>
#include <boost/lexical_cast.hpp>
#include <boost/assign/list_of.hpp>
#include "nova/guest/apt.h"
#include "nova/utils/regex.h"
#include "nova/process.h"
#include "nova/Log.h"
#include "status.h"
#include <sstream>
#include <string>
#include <sys/statvfs.h>
#include <initializer_list>

using namespace boost::assign; // brings CommandList += into our code.
using nova::guest::apt::AptGuest;
namespace process = nova::process;
using nova::utils::Regex;
using nova::utils::RegexMatchesPtr;
using std::string;
using std::stringstream;
using namespace boost;
using namespace std;

namespace nova { namespace guest { namespace monitoring {

namespace {

    const bool USE_SUDO = true;
    const char * TMP_MON_CONF = "/tmp/rackspace-monitoring-agent.cfg.tmp";
}

/**---------------------------------------------------------------------------
 *- Monitoring
 *- TODO(cp16net,tim) change classes to 'Manager'
 *---------------------------------------------------------------------------*/

MonitoringManager::MonitoringManager(const std::string & guest_id,
                       const std::string & agent_package_name,
                       const std::string & agent_config_file,
                       const double agent_install_timeout) :
    guest_id(guest_id),
    agent_package_name(agent_package_name),
    agent_config_file(agent_config_file),
    agent_install_timeout(agent_install_timeout) {
}

void MonitoringManager::ensure_running() {
    MonitoringStatus status;
    switch(status.get_current_status())
    {
        case MonitoringStatus::REMOVED:
            NOVA_LOG_DEBUG("Monitoring agent is not installed on this machine.");
            return;
        case MonitoringStatus::ACTIVE:
            NOVA_LOG_DEBUG("The monitoring agent is running.");
            return;
        case MonitoringStatus::SHUTDOWN:
            NOVA_LOG_ERROR("Monitoring agent is not running!");
            start_monitoring_agent();
            return;
        default:
            NOVA_LOG_ERROR("Bad status case for monitoring status!");
            return;
    }
}

void MonitoringManager::install_and_configure_monitoring_agent(
                AptGuest & apt,
                std::string monitoring_token,
                std::string monitoring_endpoints) const {
    NOVA_LOG_DEBUG("install_and_configure_monitoring_agent call ");
    // Writing out the monitoring config file
    ofstream config;
    config.open(TMP_MON_CONF);
    if (!config.good()) {
        NOVA_LOG_ERROR("Couldn't open config file: %s.", TMP_MON_CONF);
        throw MonitoringException(MonitoringException::CANT_WRITE_CONFIG_FILE);
    }
    NOVA_LOG_DEBUG("opening the files %s", agent_config_file);
    string buffer;
    ifstream orig_config_file;
    orig_config_file.open(agent_config_file.c_str());

    if (orig_config_file.is_open()) {

        // get the region line to append to file
        getline(orig_config_file, buffer);
        NOVA_LOG_DEBUG("buffer of orig_config_file: %s", buffer);
        config << buffer << endl;

    }
    orig_config_file.close();
    config << endl;
    config << "monitoring_id " << guest_id << endl;
    config << "monitoring_token " << monitoring_token << endl;
    if (monitoring_endpoints != "") {
        config << "monitoring_endpoints " << monitoring_endpoints << endl;
    }
    config.close();

    // Move the temp monitoring config file into place and restart agent
    NOVA_LOG_INFO("Moving tmp monitoring file into place.");
    process::execute(list_of("/usr/bin/sudo")("mv")(TMP_MON_CONF)
        (agent_config_file.c_str()));

    apt.install(agent_package_name.c_str(), agent_install_timeout);
}

void MonitoringManager::update_monitoring_agent(nova::guest::apt::AptGuest & apt) const{
    NOVA_LOG_DEBUG("update_monitoring_agent call ");
    apt.install(agent_package_name.c_str(), agent_install_timeout);
}

void MonitoringManager::remove_monitoring_agent(AptGuest & apt) const {
    stop_monitoring_agent();
    NOVA_LOG_DEBUG("remove_monitoring_agent call ");
    apt.remove(agent_package_name.c_str(), agent_install_timeout);
}

void MonitoringManager::start_monitoring_agent() const {
    NOVA_LOG_INFO("Starting monitoring agent...");
    process::execute(list_of("/usr/bin/sudo")
                            ("/etc/init.d/rackspace-monitoring-agent")
                            ("start"));
}

void MonitoringManager::stop_monitoring_agent() const {
    NOVA_LOG_INFO("Stopping monitoring agent...");
    try{
        process::execute(list_of("/usr/bin/sudo")
                                ("/etc/init.d/rackspace-monitoring-agent")
                                ("stop"));
    }
    catch (process::ProcessException &e) {
        NOVA_LOG_ERROR("Failed to stop monitoring agent: %s" , e.what());
        NOVA_LOG_ERROR("Trying to killall rackspace-monitoring-agent");
        try{
            process::execute(list_of("/usr/bin/sudo")
                                    ("killall")
                                    ("rackspace-monitoring-agent"));
        }
        catch (process::ProcessException &e) {
            NOVA_LOG_ERROR("Failed to killall monitoring agent: %s" , e.what());
            NOVA_LOG_ERROR("Giving up because nothing else we can do to stop "
                           "monitoring agent");
            throw MonitoringException(MonitoringException::ERROR_STOPPING_AGENT);
        }
    }

}

void MonitoringManager::restart_monitoring_agent() const {
    NOVA_LOG_INFO("Restarting monitoring agent...");
    process::execute(list_of("/usr/bin/sudo")
                            ("/etc/init.d/rackspace-monitoring-agent")
                            ("restart"));

}


} } } // end namespace nova::guest::diagnostics
