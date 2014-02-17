#include "pch.hpp"
#include "nova/guest/mysql/MySqlAppStatus.h"
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
using nova::datastores::DatastoreStatus;
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

namespace nova { namespace guest { namespace mysql {

MySqlAppStatus::MySqlAppStatus(ResilientSenderPtr sender,
                               bool is_mysql_installed)
:   DatastoreStatus(sender, is_mysql_installed) {
}


MySqlAppStatus::~MySqlAppStatus() {
}

DatastoreStatus::Status MySqlAppStatus::determine_actual_status() const {
    // RUNNING = We could ping mysql.
    // BLOCKED = We can't ping it, but we can see the process running.
    // CRASHED = The process is dead, but left evidence it once existed.
    // SHUTDOWN = The process is dead and never existed or cleaned itself up.
    std::stringstream out;
    try {
        execute(out, list_of("/usr/bin/sudo")("/usr/bin/mysqladmin")("ping"));
        return RUNNING;
    } catch(const process::ProcessException & pe) {
        if (pe.code != process::ProcessException::EXIT_CODE_NOT_ZERO) {
            throw pe;
        }
        try {
            execute(out, list_of("/usr/bin/sudo")("/bin/ps")("-C")
                                 ("mysqld")("h"));
            // TODO(rnirmal): Need to create new statuses for instances where
            // the mysql service is up, but unresponsive
            return BLOCKED;
        } catch(const process::ProcessException & pe) {
            if (pe.code != process::ProcessException::EXIT_CODE_NOT_ZERO) {
                throw pe;
            }
            // Figure out what the PID file would be if we started.
            // If it exists, then MySQL crashed.
            optional<string> pid_file = find_mysql_pid_file();
            if (!!pid_file && is_file(pid_file.get().c_str())) {
                return CRASHED;
            } else {
                return SHUTDOWN;
            }
        }
    }
}

// These are the normal production methods.
// By defining these the tests can mock out the dependencies.
void MySqlAppStatus::execute(stringstream & out,
                                    const process::CommandList & cmds) const {
    process::execute(out, cmds);
}

bool MySqlAppStatus::is_file(const char * file_path) const {
    return io::is_file(file_path);
}

optional<string> MySqlAppStatus::find_mysql_pid_file() const {
    stringstream out;
    try {
        execute(out, list_of("/usr/bin/sudo")("/usr/sbin/mysqld")
                            ("--print-defaults"));
    } catch(const process::ProcessException & pe) {
        NOVA_LOG_ERROR("Error running mysqld --print-defaults! %s", pe.what());
        return boost::none;
    }
    Regex pattern("--pid[_-]file=([a-z0-9A-Z/]+\\.pid)");
    RegexMatchesPtr matches = pattern.match(out.str().c_str());
    if (!matches || !matches->exists_at(1)) {
        NOVA_LOG_ERROR("No match found in output of "
                       "\"mysqld --print-defaults\".");
        NOVA_LOG_ERROR(out.str().c_str())
        return boost::none;
    }
    optional<string> rtn(matches->get(1));
    return rtn;
}

} } } // end nova::guest::mysql
