#include "nova/guest/diagnostics.h"

#include <fstream>
#include <iostream>
#include <boost/lexical_cast.hpp>
#include "nova_guest_version.hpp"
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
                int convert_value = boost::lexical_cast<int>(value);
                proccess_info->fd_size=convert_value;
            }
            else if (key == "VmSize") {
                int convert_value = boost::lexical_cast<int>(value);
                proccess_info->vm_size=convert_value;
            }
            else if (key == "VmPeak") {
                int convert_value = boost::lexical_cast<int>(value);
                proccess_info->vm_peak=convert_value;
            }
            else if (key == "VmRSS") {
                int convert_value = boost::lexical_cast<int>(value);
                proccess_info->vm_rss=convert_value;
            }
            else if (key == "VmHWM") {
                int convert_value = boost::lexical_cast<int>(value);
                proccess_info->vm_hwm=convert_value;
            }
            else if (key == "Threads") {
                int convert_value = boost::lexical_cast<int>(value);
                proccess_info->threads=convert_value;
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
