#include "pch.hpp"
#include "nova/datastores/DatastoreStatus.h"
#include <boost/format.hpp>
#include "nova/utils/io.h"
#include <boost/lexical_cast.hpp>
#include <boost/assign/list_of.hpp>
#include <boost/thread/locks.hpp>
#include "nova/Log.h"
#include <nova/db/mysql.h>
#include "nova/guest/mysql/MySqlGuestException.h"
#include <boost/optional.hpp>
#include "nova/process.h"
#include "nova/rpc/sender.h"
#include "nova/utils/regex.h"
#include <boost/thread.hpp>
#include "nova/guest/utils.h"
#include <string>
#include "nova/utils/subsecond.h"

using namespace boost::assign; // brings CommandList += into our code.
using boost::format;
using nova::json_obj;
using nova::db::mysql::MySqlConnectionWithDefaultDb;
using nova::db::mysql::MySqlConnectionWithDefaultDbPtr;
using nova::db::mysql::MySqlException;
using nova::guest::mysql::MySqlGuestException;
using nova::db::mysql::MySqlPreparedStatementPtr;
using nova::db::mysql::MySqlResultSetPtr;
using boost::optional;
namespace process = nova::process;
using nova::utils::Regex;
using nova::utils::RegexMatchesPtr;
using std::string;
using std::stringstream;
using nova::rpc::ResilientSenderPtr;
using nova::utils::subsecond::now;

namespace io = nova::utils::io;
namespace utils = nova::guest::utils;

namespace nova { namespace datastores {

DatastoreStatus::DatastoreStatus(ResilientSenderPtr sender,
                                 bool is_installed)
: installed(is_installed),
  restart_mode(false),
  status(boost::none),
  sender(sender) {
}

DatastoreStatus::~DatastoreStatus() {

}

void DatastoreStatus::begin_install() {
    boost::lock_guard<boost::mutex> lock(status_mutex);
    set_status(BUILDING);
}

void DatastoreStatus::begin_restart() {
    boost::lock_guard<boost::mutex> lock(status_mutex);
    this->restart_mode = true;;
}

void DatastoreStatus::end_failed_install() {
    boost::lock_guard<boost::mutex> lock(status_mutex);
    this->installed = false;
    this->restart_mode = false;
    this->set_status(FAILED);
}

void DatastoreStatus::end_install_or_restart() {
    boost::lock_guard<boost::mutex> lock(status_mutex);
    this->installed = true;
    this->restart_mode = false;
    set_status(determine_actual_status());
}

const char * DatastoreStatus::get_current_status_string() const {
    if (status) {
        return status_name(status.get());
    } else {
        return "none";
    }
}

bool DatastoreStatus::is_installed() const {
    return installed;
}

bool DatastoreStatus::is_restarting() const {
    return restart_mode;
}

bool DatastoreStatus::is_running() const {
    return (status && status.get() == RUNNING);
}

void DatastoreStatus::set_status(DatastoreStatus::Status status) {
    if (NEW == status) {
        NOVA_LOG_INFO("Won't update Conductor as status is NEW.");
        // The only reason for NEW is if the datastore hasn't been installed.
        // That's a somewhat rare case which normally indicates something has
        // gone wrong, such as the prepare call being delayed. So in this case
        // ignoring it can be the best option.
        return;
    }
    const char * description = status_name(status);
    NOVA_LOG_INFO("Updating app status to %d (%s).", ((int)status),
                   description);
    sender->send("heartbeat",
        "payload", json_obj(
            "service_status", description
        )
    );
    this->status = optional<int>(status);
}

const char * DatastoreStatus::status_name(DatastoreStatus::Status status) {
    // Make sure this matches the integers used by Trove!
    switch(status) {
        //! BEGIN GENERATED CODE
        case BLOCKED:
            return "blocked";
        case BUILDING:
            return "building";
        case BUILD_PENDING:
            return "build pending";
        case CRASHED:
            return "crashed";
        case DELETED:
            return "deleted";
        case FAILED:
            return "failed to spawn";
        case FAILED_TIMEOUT_GUESTAGENT:
            return "guestagent error";
        case NEW:
            return "new";
        case PAUSED:
            return "paused";
        case RUNNING:
            return "running";
        case SHUTDOWN:
            return "shutdown";
        case UNKNOWN:
            return "unknown";
        //! END GENERATED CODE
        default:
            return "!invalid status code!";
    }
}

void DatastoreStatus::update() {
    boost::lock_guard<boost::mutex> lock(status_mutex);

    if (is_installed() && !is_restarting()) {
        NOVA_LOG_TRACE("Determining status of app...");
        Status status = determine_actual_status();
        set_status(status);
    } else {
        NOVA_LOG_INFO("Datastore is not installed or is in restart mode, so for "
                      "now we'll skip determining the status of the app on this "
                      "box.");
    }
}

bool DatastoreStatus::wait_for_real_state_to_change_to(Status status,
                                                      int max_time,
                                                      bool update_db){
    boost::lock_guard<boost::mutex> lock(status_mutex);
    const int wait_time = 3;
    for (int time = 0; time < max_time; time += wait_time) {
        boost::this_thread::sleep(boost::posix_time::seconds(wait_time));
        time += 1;
        NOVA_LOG_INFO("Waiting for datastore status to change to %s...",
                       DatastoreStatus::status_name(status));
        const DatastoreStatus::Status actual_status = determine_actual_status();
        NOVA_LOG_INFO("Datastore status was %s after %d seconds.",
                       DatastoreStatus::status_name(actual_status), time);
        if (actual_status == status)
        {
            if (update_db) {
                set_status(actual_status);
            }
            return true;
        }
    }
    NOVA_LOG_ERROR("Time out while waiting for datastore app status to change!");
    return false;
}


} } // end nova::guest::mysql
