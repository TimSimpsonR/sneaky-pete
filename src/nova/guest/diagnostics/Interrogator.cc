#include "nova/guest/diagnostics.h"

#include <fstream>
#include <iostream>
#include "nova/guest/version.h"
#include "nova/utils/regex.h"
#include "nova/Log.h"
#include <sstream>
#include <string>

using nova::utils::Regex;
using nova::utils::RegexMatchesPtr;
using std::string;
using std::stringstream;
using namespace boost;
using namespace std;

namespace nova { namespace guest { namespace diagnostics {

/**---------------------------------------------------------------------------
 *- Interrogator
 *---------------------------------------------------------------------------*/

Interrogator::Interrogator(){
}

DiagInfoPtr Interrogator::get_diagnostics() const {
    NOVA_LOG_DEBUG("getDiagnostics call ");

    DiagInfoPtr map(new DiagInfo());
    int pid = (int)getpid();

    stringstream stream_proc_status;
    stream_proc_status << "/proc/" << pid << "/status";
    NOVA_LOG_DEBUG2("proc status file location : %s", 
                    stream_proc_status.str().c_str());

    string stat_line;

    ifstream status_file(stream_proc_status.str().c_str());
    if (!status_file.is_open()) {
        throw InterrogatorException(InterrogatorException::FILE_NOT_FOUND);
    }
    while (status_file.good()) {
        getline (status_file,stat_line);
        
        Regex regex("(VmSize|VmPeak|VmRSS|VmHWM|Threads):\\s+([0-9]+)");
        RegexMatchesPtr matches = regex.match(stat_line.c_str());

        if (matches) {
            NOVA_LOG_DEBUG2("line : %s", stat_line.c_str());

            if (!matches->exists_at(2)) {
                throw InterrogatorException(
                    InterrogatorException::PATTERN_DOES_NOT_MATCH);
            }

            string key;
            string value;
            key = matches->get(1);
            value = matches->get(2);
            (*map)[key] = value;

            NOVA_LOG_DEBUG2("%s : %s", key.c_str(), value.c_str());
        }
    }
    status_file.close();

    // Add the version to the map
    (*map)["version"] = NOVA_GUEST_CURRENT_VERSION;

    return map;
}

} } } // end namespace nova::guest::diagnostics
