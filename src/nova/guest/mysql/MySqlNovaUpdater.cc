#include "nova/guest/mysql/MySqlNovaUpdater.h"

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
#include "nova/utils/regex.h"
#include <boost/thread.hpp>
#include "nova/guest/utils.h"
#include <string>

using namespace boost::assign; // brings CommandList += into our code.
using boost::format;
using nova::db::mysql::MySqlConnectionWithDefaultDb;
using nova::db::mysql::MySqlConnectionWithDefaultDbPtr;
using nova::db::mysql::MySqlException;
using nova::guest::mysql::MySqlGuestException;
using nova::db::mysql::MySqlPreparedStatementPtr;
using nova::db::mysql::MySqlResultSetPtr;
using nova::guest::utils::IsoDateTime;
using nova::guest::utils::IsoTime;
using boost::optional;
using nova::Process;
using nova::ProcessException;
using nova::utils::Regex;
using nova::utils::RegexMatchesPtr;
using std::string;
using std::stringstream;

namespace io = nova::utils::io;
namespace utils = nova::guest::utils;

namespace nova { namespace guest { namespace mysql {

// These are the normal production methods.
// By defining these the tests can mock out the dependencies.
void MySqlNovaUpdaterContext::execute(stringstream & out,
                                      const Process::CommandList & cmds) const {
    Process::execute(out, cmds);
}

bool MySqlNovaUpdaterContext::is_file(const char * file_path) const {
    return io::is_file(file_path);
}

MySqlNovaUpdater::MySqlNovaUpdater(MySqlConnectionWithDefaultDbPtr
                                       nova_db_connection,
                                   const char * guest_ethernet_device,
                                   unsigned long nova_db_reconnect_wait_time,
                                   boost::optional<int> preset_instance_id,
                                   MySqlNovaUpdaterContext * context)
: context(context),
  guest_ethernet_device(guest_ethernet_device),
  nova_db(nova_db_connection),
  nova_db_mutex(),
  nova_db_reconnect_wait_time(nova_db_reconnect_wait_time),
  preset_instance_id(preset_instance_id),
  status(boost::none)
{
    struct F : MySqlNovaUpdaterFunctor {
        MySqlNovaUpdater * updater;

        virtual void operator()() {
            updater->status = updater->get_status_from_nova_db();
        }
    };
    F f;
    f.updater = this;
    repeatedly_attempt_mysql_method(f);
}

void MySqlNovaUpdater::begin_mysql_install() {
    boost::lock_guard<boost::mutex> lock(nova_db_mutex);
    ensure_db();
    set_status(BUILDING);
}

void MySqlNovaUpdater::ensure_db() {
    nova_db->ensure();
}

optional<string> MySqlNovaUpdater::find_mysql_pid_file() const {
    stringstream out;
    try {
        context->execute(out, list_of("/usr/bin/sudo")("/usr/sbin/mysqld")
                        ("--print-defaults"));
    } catch(const ProcessException & pe) {
        NOVA_LOG_ERROR2("Error running mysqld --print-defaults! %s", pe.what());
        return boost::none;
    }
    Regex pattern("--pid-file=([a-z0-9A-Z/]+\\.pid)");
    RegexMatchesPtr matches = pattern.match(out.str().c_str());
    if (!matches || !matches->exists_at(1)) {
        NOVA_LOG_ERROR("No match found in output of "
                       "\"mysqld --print-defaults\".");
        return boost::none;
    }
    optional<string> rtn(matches->get(1));
    return rtn;
}

MySqlNovaUpdater::Status MySqlNovaUpdater::get_actual_db_status() const {
    // RUNNING = We could ping mysql.
    // BLOCKED = We can't ping it, but we can see the process running.
    // CRASHED = The process is dead, but left evidence it once existed.
    // SHUTDOWN = The process is dead and never existed or cleaned itself up.
    std::stringstream out;
    try {
        context->execute(out, list_of("/usr/bin/sudo")("/usr/bin/mysqladmin")
                             ("ping"));
        return RUNNING;
    } catch(const ProcessException & pe) {
        if (pe.code != ProcessException::EXIT_CODE_NOT_ZERO) {
            throw pe;
        }
        try {
            context->execute(out, list_of("/usr/bin/sudo")("/bin/ps")("-C")
                                 ("mysqld")("h"));
            // TODO(rnirmal): Need to create new statuses for instances where
            // the mysql service is up, but unresponsive
            return BLOCKED;
        } catch(const ProcessException & pe) {
            if (pe.code != ProcessException::EXIT_CODE_NOT_ZERO) {
                throw pe;
            }
            // Figure out what the PID file would be if we started.
            // If it exists, then MySQL crashed.
            optional<string> pid_file = find_mysql_pid_file();
            if (!!pid_file && context->is_file(pid_file.get().c_str())) {
                return CRASHED;
            } else {
                return SHUTDOWN;
            }
        }
    }
}

int MySqlNovaUpdater::get_guest_instance_id() {
    if (preset_instance_id) {
        return preset_instance_id.get();
    }
    string address = utils::get_ipv4_address(guest_ethernet_device.c_str());

    MySqlPreparedStatementPtr stmt = nova_db->prepare_statement(
        "SELECT instance_id FROM fixed_ips WHERE address=? ");
    stmt->set_string(0, address.c_str());
    MySqlResultSetPtr result = stmt->execute(1);
    if (!result->next()) {
        NOVA_LOG_ERROR2("Could not find guest instance for host given address "
                        "%s.", address.c_str());
        throw MySqlGuestException(MySqlGuestException::GUEST_INSTANCE_ID_NOT_FOUND);
    }
    optional<string> id = result->get_string(0);
    NOVA_LOG_DEBUG2("instance from db=%s", id.get().c_str());
    return boost::lexical_cast<int>(id.get().c_str());
}

optional<MySqlNovaUpdater::Status> MySqlNovaUpdater::get_status_from_nova_db() {
    int instance_id = get_guest_instance_id();
    MySqlPreparedStatementPtr stmt = nova_db->prepare_statement(
        "SELECT state FROM guest_status WHERE instance_id= ? ");
    stmt->set_int(0, instance_id);
    MySqlResultSetPtr result = stmt->execute(1);
    if (result->next()) {
        optional<int> status_as_int = result->get_int(0);
        if (status_as_int) {
            Status status = (Status) status_as_int.get();
            return optional<Status>(status);
        }
    }
    return boost::none;
}

void MySqlNovaUpdater::mark_mysql_as_installed() {
    boost::lock_guard<boost::mutex> lock(nova_db_mutex);

    struct F : MySqlNovaUpdaterFunctor {
        Status status;
        MySqlNovaUpdater * updater;

        virtual void operator()() {
            updater->set_status(status);
        }
    };
    F f;
    f.status = get_actual_db_status();
    f.updater = this;
    // f->updater = this;
    // f->status = get_actual_db_status();
    repeatedly_attempt_mysql_method(f);
}

bool MySqlNovaUpdater::mysql_is_installed() {
    return (status && status.get() != BUILDING && status.get() != FAILED);
}

void MySqlNovaUpdater::repeatedly_attempt_mysql_method(
    MySqlNovaUpdaterFunctor & f)
{
    bool success = false;
    while(!success){
        try {
            this->ensure_db();
            f();
            success = true;
        } catch(const MySqlException & mse) {
            NOVA_LOG_ERROR2("Error communicating to Nova database: %s", mse.what());
            NOVA_LOG_ERROR2("Waiting %lu seconds to try again...",
                            nova_db_reconnect_wait_time);
            success = false;
            boost::posix_time::seconds time(nova_db_reconnect_wait_time);
            boost::this_thread::sleep(time);
            NOVA_LOG_ERROR("... trying again.");
        }
    }
}

void MySqlNovaUpdater::set_status(MySqlNovaUpdater::Status status) {
    int instance_id = get_guest_instance_id();

    const char * description = status_name(status);
    Status state = status;
    NOVA_LOG_INFO2("Updating MySQL app status to %d (%s).", ((int)status),
                   description);
    IsoDateTime now;
    MySqlPreparedStatementPtr stmt;
    if (get_status_from_nova_db() == boost::none) {
        NOVA_LOG_INFO("Inserting new Guest status row. Why wasn't this there?");
        stmt = nova_db->prepare_statement(
            "INSERT INTO guest_status "
            "(created_at, updated_at, instance_id, state, state_description) "
            "VALUES(?, ?, ?, ?, ?) ");
        stmt->set_string(0, now.c_str());
        stmt->set_string(1, now.c_str());
        stmt->set_int(2, instance_id);
        stmt->set_int(3, (int)state);
        stmt->set_string(4, description);
    } else {
        stmt = nova_db->prepare_statement(
            "UPDATE guest_status "
            "SET state_description=?, state=?, updated_at=? "
            "WHERE instance_id=?");
        stmt->set_string(0, description);
        stmt->set_int(1, (int) state);
        stmt->set_string(2, now.c_str());
        stmt->set_int(3, instance_id);
    }
    MySqlResultSetPtr result = stmt->execute(0);
    this->status = optional<int>(status);
}

const char * MySqlNovaUpdater::status_name(MySqlNovaUpdater::Status status) {
    // Make sure this matches nova.compute.power_state!
    switch(status) {
        case BUILDING:
            return "building";
        case BLOCKED:
            return "blocked";
        case CRASHED:
            return "crashed";
        case FAILED:
            return "failed";
        case RUNNING:
            return "running";
        case SHUTDOWN:
            return "shutdown";
        default:
            return "unknown_case";
    }
}

void MySqlNovaUpdater::update() {
    boost::lock_guard<boost::mutex> lock(nova_db_mutex);
    ensure_db();

    if (mysql_is_installed()) {
        NOVA_LOG_INFO("Determining status of MySQL app...");
        Status status = get_actual_db_status();
        set_status(status);
    } else {
        NOVA_LOG_INFO("MySQL is not installed or was no call to prepare was "
                      "made, so for now we'll skip determining the status of "
                      "MySQL on this box.");
    }
}


} } } // end nova::guest::mysql
