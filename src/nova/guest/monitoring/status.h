#ifndef __NOVA_GUEST_MONITORING_STATUS_H
#define __NOVA_GUEST_MONITORING_STATUS_H

#include <boost/optional.hpp>
#include <boost/shared_ptr.hpp>
#include "nova/guest/apt.h"
#include "nova/guest/guest.h"
#include <map>
#include <vector>
#include <string>
#include <boost/tuple/tuple.hpp>
#include <boost/utility.hpp>


namespace nova { namespace guest { namespace monitoring { namespace status {

    class MonitoringStatus : boost::noncopyable {
        public:
            enum Status {
                ACTIVE = 0x01, // Monitoring Agent is active.
                REMOVED = 0X02, // Monitoring Agent is not installed.
                SHUTDOWN = 0x03, // Monitoring Agent is not running.
                BUILDING = 0x05, // Monitoring Agent is being installed / prepared.
                UNKNOWN=0x06,  // Set when the agent becomes unresponsive.
            };

            MonitoringStatus();

            /** Grabs the installed version info and the status.
             *  Returns boost::none if the agent is not installed. */
            boost::optional<boost::tuple<std::string, Status> >
                get_monitoring_status(nova::guest::apt::AptGuest & apt);

            /** Gets the status for monitoring. */
            Status get_current_status();

            /** Returns a readable string for each status enum. */
            static const char * status_name(Status status);
    };

} } } }  // end namespace

#endif //__NOVA_GUEST_MONITORING_STATUS_H
