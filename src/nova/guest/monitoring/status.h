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


namespace nova { namespace guest { namespace monitoring {

    class MonitoringStatus : boost::noncopyable {
        public:
            enum Status {
                ACTIVE = 0x01, // Monitoring Agent is active.
                REMOVED = 0X02, // Monitoring Agent is not installed.
                SHUTDOWN = 0x03 // Monitoring Agent is not running.
            };

            //TODO(tim.simpson): Keeping this a class as some of the constants
            //                   it uses may need to become class fields,
            //                   passed in initially.
            MonitoringStatus();

            /** Grabs the installed version info and the status.
             *  Returns boost::none if the agent is not installed. */
            boost::optional<boost::tuple<std::string, Status> >
                get_monitoring_status(nova::guest::apt::AptGuest & apt);

            /** Gets the status for monitoring. */
            Status get_current_status();

            /** Returns a readable string for each status enum. */
            static const char * status_name(Status status);

        private:

            static boost::optional<std::string> read_pid_from_file();
    };

} } }  // end namespace

#endif //__NOVA_GUEST_MONITORING_STATUS_H
