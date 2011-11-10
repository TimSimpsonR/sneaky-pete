#include "nova/guest/mysql/MySqlNovaUpdater.h"

#include <boost/format.hpp>
#include "nova/utils/io.h"
#include <boost/lexical_cast.hpp>
#include <boost/assign/list_of.hpp>
#include "nova/Log.h"
#include "nova/guest/mysql/MySqlGuestException.h"
#include <boost/optional.hpp>
#include "nova/process.h"
#include "nova/utils/regex.h"
#include "nova/guest/utils.h"
#include <string>

using namespace boost::assign; // brings CommandList += into our code.
using boost::format;
using nova::db::mysql::MySqlConnection;
using nova::db::mysql::MySqlConnectionPtr;
using nova::guest::mysql::MySqlGuestException;
using nova::db::mysql::MySqlPreparedStatementPtr;
using nova::db::mysql::MySqlResultSetPtr;
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

MySqlNovaUpdater::MySqlNovaUpdater(MySqlConnectionPtr nova_db_connection,
                                   const char * guest_ethernet_device)
: guest_ethernet_device(guest_ethernet_device), nova_db(nova_db_connection) {
}

optional<string> MySqlNovaUpdater::find_mysql_pid_file() const {
    Log log;
    stringstream out;
    try {
        Process::execute(out, list_of("/usr/bin/sudo")("/usr/sbin/mysqld")
                              ("--print-defaults"));
    } catch(const ProcessException & pe) {
        log.error2("Error running mysqld --print-defaults! %s", pe.what());
        return boost::none;
    }
    Regex pattern("--pid-file=([\\w/]+\\.pid)[\\s]");
    RegexMatchesPtr matches = pattern.match(out.str().c_str(), 1);
    if (!matches || !matches->exists_at(1)) {
        log.error("No match found in output of \"mysqld --print-defaults\".");
        return boost::none;
    }
    optional<string> rtn(matches->get(1));
    return rtn;
}

int MySqlNovaUpdater::get_guest_instance_id() {
    Log log;
    string address = utils::get_ipv4_address(guest_ethernet_device.c_str());

    MySqlPreparedStatementPtr stmt = nova_db->prepare_statement(
        "SELECT instance_id FROM fixed_ips WHERE address= ? ");
    stmt->set_string(0, address.c_str());
    MySqlResultSetPtr result = stmt->execute(1);
    if (!result->next()) {
        throw MySqlGuestException(MySqlGuestException::GUEST_INSTANCE_ID_NOT_FOUND);
    }
    optional<string> id = result->get_string(0);
    log.debug("instance from db=%s", id.get().c_str());
    return boost::lexical_cast<int>(id.get().c_str());
}

MySqlNovaUpdater::Status MySqlNovaUpdater::get_local_db_status() const {
    // RUNNING = We could ping mysql.
    // BLOCKED = We can't ping it, but we can see the process running.
    // CRASHED = The process is dead, but left evidence it once existed.
    // SHUTDOWN = The process is dead and never existed or cleaned itself up.
    try {
        Process::execute(list_of("/usr/bin/sudo")("/usr/bin/mysqladmin")
                         ("ping"));
        return RUNNING;
    } catch(const ProcessException & pe) {
        if (pe.code != ProcessException::EXIT_CODE_NOT_ZERO) {
            throw pe;
        }
        try {
            Process::execute(list_of("/usr/bin/sudo")("/bin/ps")("-C")
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
            if (!!pid_file && io::is_file(pid_file.get().c_str())) {
                return CRASHED;
            } else {
                return SHUTDOWN;
            }
        }
    }
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
        case RUNNING:
            return "running";
        case SHUTDOWN:
            return "shutdown";
        default:
            return "unknown_case";
    }
}


void MySqlNovaUpdater::update_status(MySqlNovaUpdater::Status status) {
    string instance_id = str(format("%d") % get_guest_instance_id());
    const char * description = status_name(status);
    string state = str(format("%d") % ((int)status));

    MySqlPreparedStatementPtr stmt = nova_db->prepare_statement(
        "UPDATE guest_status SET state_description=?, state=? "
        "WHERE instance_id=?;");
    stmt->set_string(0, description);
    stmt->set_string(1, state.c_str());
    stmt->set_string(2, instance_id.c_str());
    MySqlResultSetPtr result = stmt->execute(0);
}


} } } // end nova::guest::mysql
