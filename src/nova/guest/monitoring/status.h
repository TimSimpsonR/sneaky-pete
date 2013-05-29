#ifndef __NOVA_GUEST_MONITORING_STATUS_H
#define __NOVA_GUEST_MONITORING_STATUS_H

#include <boost/optional.hpp>
#include <boost/shared_ptr.hpp>
#include "nova/guest/apt.h"
#include "nova/guest/guest.h"
#include <map>
#include <vector>
#include <string>

using namespace std;

namespace nova { namespace guest { namespace monitoring { namespace status {

    struct MonitoringStatusInfo
    {
        boost::optional<string> version;
        boost::optional<string> status;
    };

    typedef boost::shared_ptr<MonitoringStatusInfo> MonitoringInfoPtr;

    class MonitoringStatus {
        public:
            enum Status {
                ACTIVE = 0x01, // Monitoring Agent is active.
                REMOVED = 0X02, // Monitoring Agent is not installed.
                SHUTDOWN = 0x03, // Monitoring Agent is not running.
                BUILDING = 0x05, // Monitoring Agent is being installed / prepared.
                UNKNOWN=0x06,  // Set when the agent becomes unresponsive.
            };

            MonitoringStatus();

            /** Grabs monitoring for this program. */
            static MonitoringInfoPtr get_monitoring_status(
                nova::guest::apt::AptGuest & apt);

            /** Useful for diagnostics and logging. */
            const char * get_current_status_string(boost::optional<string> version);

            /** Returns a readable string for each status enum. */
            static const char * status_name(Status status);

        private:
            MonitoringStatus(MonitoringStatus const &);
            MonitoringStatus & operator = (const MonitoringStatus &);

            void find_monitoring_agent_status();

            boost::optional<Status> status;


    };

} } } }  // end namespace

#endif //__NOVA_GUEST_MONITORING_STATUS_H
