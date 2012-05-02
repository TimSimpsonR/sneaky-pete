#include "nova/guest/diagnostics.h"

#include <boost/format.hpp>
#include <fstream>
#include <iostream>
#include <boost/lexical_cast.hpp>
#include "nova_guest_version.hpp"
#include "nova/utils/regex.h"
#include "nova/Log.h"
#include <sstream>
#include <string>
#include <sys/statvfs.h>

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

DiagInfoPtr Interrogator::get_diagnostics() {
    NOVA_LOG_DEBUG("getDiagnostics call ");

    DiagInfo * process_info;
    DiagInfoPtr retValue(process_info=new DiagInfo());
    pid_t pid = getpid();

    ProcStatus * status = dynamic_cast<ProcStatus *>(process_info);
    get_proc_status(pid, *status);

    // Add the version to the Info
    process_info->version = NOVA_GUEST_CURRENT_VERSION;

    return retValue;
}

void Interrogator::get_proc_status(pid_t pid, ProcStatus & process_info) {
    string proc_status_file = str(format("/proc/%s/status") % pid);
    NOVA_LOG_DEBUG2("proc status file location : %s",
                    proc_status_file.c_str());

    string stat_line;

    ifstream status_file(proc_status_file.c_str());
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
                process_info.fd_size=convert_value;
            }
            else if (key == "VmSize") {
                int convert_value = boost::lexical_cast<int>(value);
                process_info.vm_size=convert_value;
            }
            else if (key == "VmPeak") {
                int convert_value = boost::lexical_cast<int>(value);
                process_info.vm_peak=convert_value;
            }
            else if (key == "VmRSS") {
                int convert_value = boost::lexical_cast<int>(value);
                process_info.vm_rss=convert_value;
            }
            else if (key == "VmHWM") {
                int convert_value = boost::lexical_cast<int>(value);
                process_info.vm_hwm=convert_value;
            }
            else if (key == "Threads") {
                int convert_value = boost::lexical_cast<int>(value);
                process_info.threads=convert_value;
            }

            NOVA_LOG_DEBUG2("%s : %s", key.c_str(), value.c_str());
        }
    }
    status_file.close();
}

FileSystemStatsPtr Interrogator::get_filesystem_stats(std::string fs_path) {
    NOVA_LOG_DEBUG2("Retrieving FileSystem Stats for : %s", fs_path.c_str());
    struct statvfs stats_buf;
    FileSystemStatsPtr fs_stats(new FileSystemStats());
    if (!statvfs(fs_path.c_str(), &stats_buf)) {
        fs_stats->block_size = stats_buf.f_bsize;
        fs_stats->total_blocks = stats_buf.f_blocks;
        fs_stats->free_blocks = stats_buf.f_bfree;
        fs_stats->total = fs_stats->total_blocks * fs_stats->block_size;
        fs_stats->free = fs_stats->free_blocks * fs_stats->block_size;
        fs_stats->used = fs_stats->total - fs_stats->free;
        return fs_stats;
    } else {
        throw InterrogatorException(InterrogatorException::FILESYSTEM_NOT_FOUND);
    }
}

} } } // end namespace nova::guest::diagnostics
