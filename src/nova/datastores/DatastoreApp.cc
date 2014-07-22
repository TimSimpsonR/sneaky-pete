#include "pch.hpp"
#include "DatastoreApp.h"

#include <boost/assign/list_of.hpp>
#include "nova/process.h"
#include "DatastoreException.h"

using namespace boost::assign; // brings CommandList += into our code.
using std::stringstream;
using namespace nova::process;

namespace nova { namespace datastores {

namespace {
    const char * const DEFAULTS = "defaults";
    const char * const DISABLE = "disable";
    const char * const ENABLE = "enable";
} // end anonymous namespace

/**---------------------------------------------------------------------------
 *- StartOnBoot
 *---------------------------------------------------------------------------*/

DatastoreApp::StartOnBoot::StartOnBoot(const char * const service_name)
:   service_name(service_name)
{
}

void DatastoreApp::StartOnBoot::call_update_rc(
    const char * const arg, bool throw_on_bad_exit_code)
{
    // update-rc.d is typically fast and will return exit code 0 normally,
    // even if it is enabling or disabling mysql twice in a row.
    // UPDATE: Nope, turns out this is only on my box. Sometimes it
    //         returns something else.
    stringstream output;
    process::CommandList cmds = list_of("/usr/bin/sudo")
        ("update-rc.d")(service_name)(arg);
    try {
        process::execute(output, cmds);
    } catch(const process::ProcessException & pe) {
        NOVA_LOG_ERROR("Exception running process!");
        NOVA_LOG_ERROR(pe.what());
        if (throw_on_bad_exit_code) {
            NOVA_LOG_ERROR(output.str().c_str());
            throw;
        }
    }
    NOVA_LOG_INFO(output.str().c_str());
}
void DatastoreApp::StartOnBoot::disable_or_throw() {
    call_update_rc(DISABLE, true);
}

void DatastoreApp::StartOnBoot::enable_maybe() {
    call_update_rc(ENABLE, false);
}

void DatastoreApp::StartOnBoot::enable_or_throw() {
    call_update_rc(ENABLE, true);
}

void DatastoreApp::StartOnBoot::setup_defaults() {
    call_update_rc(DEFAULTS, true);
}

DatastoreApp::DatastoreApp(const char * const service_name,
                           DatastoreStatusPtr status,
                           const int state_change_wait_time)
:   status(status),
    state_change_wait_time(state_change_wait_time),
    start_on_boot(service_name)
{
}

DatastoreApp::~DatastoreApp() {
}

void DatastoreApp::internal_start(const bool update_trove) {
    NOVA_LOG_INFO("Starting datastore.");
    specific_start_app_method();
    wait_for_internal_start(update_trove);
    NOVA_LOG_INFO("Datastore stopped.");
}

void DatastoreApp::internal_stop(const bool update_trove) {
    NOVA_LOG_INFO("Stopping datastore.");
    specific_stop_app_method();
    wait_for_internal_stop(update_trove);
    NOVA_LOG_INFO("Datastore stopped.");
}

void DatastoreApp::restart() {
    NOVA_LOG_INFO("Beginning restart.");
    struct Restarter {
        DatastoreStatusPtr & status;

        Restarter(DatastoreStatusPtr & status)
        :   status(status) {
            status->begin_restart();
        }

        ~Restarter() {
            // Make sure we end this, even if the result is a failure.
            status->end_install_or_restart();
        }
    } restarter(status);
    internal_stop(false);
    // In case the start fails, at least make sure it starts up on boot.
    start_on_boot.enable_maybe();
    internal_start(false);
    NOVA_LOG_INFO("Datastore app restarted successfully.");
}

void DatastoreApp::stop(bool do_not_start_on_reboot) {
    if (do_not_start_on_reboot) {
        start_on_boot.disable_or_throw();
    }
    internal_stop(true);
}

void DatastoreApp::wait_for_internal_start(const bool update_trove) {
    if (!status->wait_for_real_state_to_change_to(
        DatastoreStatus::RUNNING, this->state_change_wait_time, update_trove)) {
        NOVA_LOG_ERROR("Start up of Datastore failed!");
        status->end_install_or_restart();
        throw DatastoreException(DatastoreException::COULD_NOT_START);
    }
}

void DatastoreApp::wait_for_internal_stop(bool update_db) {
    if (!status->wait_for_real_state_to_change_to(
        DatastoreStatus::SHUTDOWN, this->state_change_wait_time, update_db)) {
        NOVA_LOG_ERROR("Could not stop MySQL!");
        status->end_install_or_restart();
        throw DatastoreException(DatastoreException::COULD_NOT_STOP);
    }
}

} } // end nova::datastores
