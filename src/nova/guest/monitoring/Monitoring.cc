#include "nova/guest/monitoring/monitoring.h"
#include <boost/format.hpp>
#include <fstream>
#include <iostream>
#include <boost/lexical_cast.hpp>
#include <boost/assign/list_of.hpp>
#include "nova_guest_version.hpp"
#include "nova/guest/apt.h"
#include "nova/utils/regex.h"
#include "nova/process.h"
#include "nova/Log.h"
#include <sstream>
#include <string>
#include <sys/statvfs.h>
#include <initializer_list>

using namespace boost::assign; // brings CommandList += into our code.
using nova::guest::apt::AptGuest;
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

Monitoring::Monitoring(const std::string & guest_id,
                       const std::string & agent_package_name,
                       const std::string & agent_config_file,
                       const double agent_install_timeout) :
    guest_id(guest_id),
    agent_package_name(agent_package_name),
    agent_config_file(agent_config_file),
    agent_install_timeout(agent_install_timeout) {
}


void Monitoring::install_and_configure_monitoring_agent(
                AptGuest & apt,
                std::string monitoring_token,
                std::string monitoring_endpoints) const {
    NOVA_LOG_DEBUG("install_and_configure_monitoring_agent call ");
    // Writing out the monitoring config file
    ofstream config;
    config.open(TMP_MON_CONF);
    if (!config.good()) {
        NOVA_LOG_ERROR2("Couldn't open config file: %s.", TMP_MON_CONF);
        throw MonitoringException(MonitoringException::CANT_WRITE_CONFIG_FILE);
    }
    config << "monitoring_id " << guest_id << endl;
    config << "monitoring_token " << monitoring_token << endl;
    if (monitoring_endpoints != "") {
        config << "monitoring_endpoints " << monitoring_endpoints << endl;
    }
    config.close();

    // Move the temp monitoring config file into place and restart agent
    NOVA_LOG_INFO("Moving tmp monitoring file into place.");
    Process::execute(list_of("/usr/bin/sudo")("mv")(TMP_MON_CONF)(agent_config_file.c_str()));

    apt.install(agent_package_name.c_str(), agent_install_timeout);

    try{
        NOVA_LOG_INFO("Restarting the monitoring agent");
        restart_monitoring_agent();
    }
    catch (std::exception &e) {
        NOVA_LOG_ERROR2("Problem restarting the monitoring agent: %s", e.what());
        throw;
    }
}

void Monitoring::remove_monitoring_agent(AptGuest & apt) const {
    stop_monitoring_agent();
    NOVA_LOG_DEBUG("remove_monitoring_agent call ");
    apt.remove(agent_package_name.c_str(), agent_install_timeout);
}

void Monitoring::start_monitoring_agent() const {
    NOVA_LOG_INFO("Starting monitoring agent...");
    Process::execute(list_of("/usr/bin/sudo")
                            ("/etc/init.d/rackspace-monitoring-agent")
                            ("start"));
}

void Monitoring::stop_monitoring_agent() const {
    NOVA_LOG_INFO("Stopping monitoring agent...");
    try{
        Process::execute(list_of("/usr/bin/sudo")
                                ("/etc/init.d/rackspace-monitoring-agent")
                                ("stop"));
    }
    catch (ProcessException &e) {
        NOVA_LOG_ERROR2("Failed to stop monitoring agent: %s" , e.what());
        NOVA_LOG_ERROR("Trying to killall rackspace-monitoring-agent");
        try{
            Process::execute(list_of("/usr/bin/sudo")
                                    ("killall")
                                    ("rackspace-monitoring-agent"));
        }
        catch (ProcessException &e) {
            NOVA_LOG_ERROR2("Failed to killall monitoring agent: %s" , e.what());
            NOVA_LOG_ERROR("Giving up because nothing else we can do to stop monitoring agent");
            throw MonitoringException(MonitoringException::ERROR_STOPPING_AGENT);
        }
    }

}

void Monitoring::restart_monitoring_agent() const {
    NOVA_LOG_INFO("Restarting monitoring agent...");
    Process::execute(list_of("/usr/bin/sudo")
                            ("/etc/init.d/rackspace-monitoring-agent")
                            ("restart"));

}


} } } // end namespace nova::guest::diagnostics
