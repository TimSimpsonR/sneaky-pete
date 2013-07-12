#include "pch.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include "nova/guest/monitoring/monitoring.h"
#include "nova/guest/monitoring/status.h"
#include "nova/guest/apt.h"
#include "nova/Log.h"

using namespace nova;
using namespace std;
using nova::guest::monitoring::status::MonitoringStatus;
using nova::guest::monitoring::Monitoring;
using nova::guest::apt::AptGuest;


void wait_for_enter_key() {
    string unused_input;
    cerr << "Hit enter to proceed.";
    cin >> unused_input;
}


int main(int argc, char* argv[]) {

    if (argc < 2 || argc > 3) {
        cout << "Usage: " << (argc < 1 ? "./senti_pete" : argv[0])
             << " [install|remove|status|start|stop|restart] [wait]" << endl;
        exit(1);
    }

    const string action(argv[1]);
    cout << "run the action for " << action << endl;
    string wait = "";
    if (argc > 2) {
         wait = argv[2];
    }

    LogApiScope log(LogOptions::simple());

    cout << endl;
    cout << "Testing out the installation and "
            "configuration of the monitoring agent." << endl;
    cout << endl;

    const bool use_sudo = true;
    const double time_out = 500;
    const string package_name = "rackspace-monitoring-agent";
    const string monitoring_config = "/etc/rackspace-monitoring-agent.cfg";
    const string monitoring_endpoints = "X";
    const string instance_id = "test-1234-1234";
    AptGuest apt(use_sudo, package_name.c_str(), time_out);
    Monitoring mon(instance_id, package_name, monitoring_config, time_out);


    if (action == "install"){
        // Installing the monitoring agent
        cout << "installing the monitoring agent." << endl;
        mon.install_and_configure_monitoring_agent(apt, "monitoring-agent-token", monitoring_endpoints);
    }
    else if (action == "remove"){
        // removing the monitoring agent
        cout << "removing the monitoring agent." << endl;
        mon.remove_monitoring_agent(apt);
        cout << "rackspace-monitoring-agent is removed." << endl;

    }
    else if (action == "status"){
        // Get information on the monitoring agent
        cout << "getting the status of the monitoring agent." << endl;
        MonitoringStatus status;
        auto result = status.get_monitoring_status(apt);
        if (!result) {
            cout << "the rackspace-monitoring-agent is not installed." << endl;
        } else {
            string version;
            MonitoringStatus::Status status;
            boost::tie(version, status) = result.get();
            cout << "package installed of rackspace-monitoring-agent is: " << endl;
            cout << "version: " << result->get<0>() << endl;
            cout << "status: " << result->get<1>() << endl;
            cout << endl;
        }

    }
    else if (action == "start"){
        cout << "starting the monitoring agent." << endl;
        mon.start_monitoring_agent();
    }
    else if (action == "stop"){
        cout << "stopping the monitoring agent." << endl;
        mon.stop_monitoring_agent();
    }
    else if (action == "restart"){
        cout << "restarting the monitoring agent." << endl;
        mon.restart_monitoring_agent();
    }
    else {
        cout << "action(" << action << ") not found in list." <<  endl;
        cout << "Usage: ./senti_pete [install|remove|status|start|stop|restart]" << endl;
        exit(1);
    }

    if (wait == "wait") {
        wait_for_enter_key();
    }
}
