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

    DiagInfo *proccess_info;
    DiagInfoPtr retValue(proccess_info=new DiagInfo());
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
        
        Regex regex("(FDSize|VmPeak|VmSize|VmHWM|VmRSS|Threads):\\s+([0-9]+)");
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

            if (key == "FDSize") {
                proccess_info->fd_size=value;
            }
            else if (key == "VmSize") {
                proccess_info->vm_size=value;
            }
            else if (key == "VmPeak") {
                proccess_info->vm_peak=value;
            }
            else if (key == "VmRSS") {
                proccess_info->vm_rss=value;
            }
            else if (key == "VmHWM") {
                proccess_info->vm_hwm=value;
            }
            else if (key == "Threads") {
                proccess_info->threads=value;
            }

            NOVA_LOG_DEBUG2("%s : %s", key.c_str(), value.c_str());
        }
    }
    status_file.close();

    // Add the version to the Info
    proccess_info->version = NOVA_GUEST_CURRENT_VERSION;

    return retValue;
}

} } } // end namespace nova::guest::diagnostics
