#include "pch.hpp"
#include "DatastoreApp.h"

#include <boost/assign/list_of.hpp>
#include "nova/process.h"

using namespace boost::assign; // brings CommandList += into our code.
using std::stringstream;
using namespace nova::process;

namespace nova { namespace datastores {

namespace {


} // end anonymous namespace

/**---------------------------------------------------------------------------
 *- StartOnBoot
 *---------------------------------------------------------------------------*/

DatastoreApp::StartOnBoot::StartOnBoot(const char * const service_name)
:   service_name(service_name)
{
}

void DatastoreApp::StartOnBoot::call_update_rc(
    bool enabled, bool throw_on_bad_exit_code)
{
    // update-rc.d is typically fast and will return exit code 0 normally,
    // even if it is enabling or disabling mysql twice in a row.
    // UPDATE: Nope, turns out this is only on my box. Sometimes it
    //         returns something else.
    stringstream output;
    process::CommandList cmds = list_of("/usr/bin/sudo")
        ("update-rc.d")(service_name)(enabled ? "enable" : "disable");
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
    call_update_rc(false, true);
}

void DatastoreApp::StartOnBoot::enable_maybe() {
    call_update_rc(true, false);
}

void DatastoreApp::StartOnBoot::enable_or_throw() {
    call_update_rc(true, true);
}

DatastoreApp::DatastoreApp(const char * const service_name)
:   start_on_boot(service_name)
{
}

DatastoreApp::~DatastoreApp() {
}


} } // end nova::datastores
