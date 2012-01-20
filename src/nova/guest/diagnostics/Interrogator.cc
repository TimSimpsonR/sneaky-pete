#include "nova/guest/diagnostics.h"

#include <fstream>
#include <iostream>
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

Interrogator::Interrogator(bool with_sudo)
: with_sudo(with_sudo) {
}

DiagInfoPtr Interrogator::get_diagnostics(double time_out) const {
    NOVA_LOG_DEBUG2("getDiagnostics call with time_out=%f", time_out);

    DiagInfoPtr map(new DiagInfo());
    int pid = (int)getpid();

    stringstream stream_procStatus;
    stream_procStatus << "/proc/" << pid << "/status";
    NOVA_LOG_DEBUG2("proc status file location : %s", 
                    stream_procStatus.str().c_str());

    string stat_line;

    ifstream statusFile(stream_procStatus.str().c_str());
    if (!statusFile.is_open()) {
        throw InterrogatorException(InterrogatorException::FILE_NOT_FOUND);
    }
    while (statusFile.good()) {
        getline (statusFile,stat_line);
        
        Regex regex("(VmSize|VmPeak|VmRSS|VmHWM|Threads):\\s+([0-9]+)");
        RegexMatchesPtr matches = regex.match(stat_line.c_str());

        if (matches) {
            NOVA_LOG_DEBUG2("line : %s", stat_line.c_str());

            for (int i = 0; i < 3; ++i) {
                if (!matches->exists_at(i)) {
                    throw InterrogatorException(
                        InterrogatorException::PATTERN_DOES_NOT_MATCH);
                }
            }

            string key;
            string value;
            key = matches->get(1);
            value = matches->get(2);
            (*map)[key] = value;

            NOVA_LOG_DEBUG2("%s : %s", key.c_str(), value.c_str());
        }
    }
    statusFile.close();

    return map;
}

} } } // end namespace nova::guest::diagnostics
