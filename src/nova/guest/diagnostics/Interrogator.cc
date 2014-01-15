#include "pch.hpp"
#include "nova/guest/diagnostics.h"

#include <boost/format.hpp>
#include <fstream>
#include <iostream>
#include <boost/lexical_cast.hpp>
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

ProcStatus ProcStatus::operator - (const ProcStatus & rhs) const
{
    return ProcStatus {
        fd_size - rhs.fd_size,
        vm_size - rhs.vm_size,
        vm_peak - rhs.vm_peak,
        vm_rss - rhs.vm_rss ,
        vm_hwm - rhs.vm_hwm ,
        threads - rhs.threads
    };
}

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
    process_info->version = NOVA_GUEST_VERSION;

    return retValue;
}

HwInfoPtr Interrogator::get_hwinfo() {
    NOVA_LOG_DEBUG("get_hwinfo call");

    HwInfo * hwinfo;
    HwInfoPtr hwinfo_ptr(hwinfo=new HwInfo());

    hwinfo->mem_total = get_mem_total();
    hwinfo->num_cpus = get_num_cpus();

    return hwinfo_ptr;
}

int Interrogator::get_mem_total() {
    string proc_meminfo_file = "/proc/meminfo";
    NOVA_LOG_DEBUG("getting memory info from : %s", proc_meminfo_file.c_str());

    int mem_total = 0;
    string mem_total_line;

    ifstream meminfo_file(proc_meminfo_file.c_str());
    if (!meminfo_file.is_open()) {
        throw InterrogatorException(InterrogatorException::FILE_NOT_FOUND);
    }
    while (meminfo_file.good()) {
        getline (meminfo_file,mem_total_line);

        Regex regex("(MemTotal):\\s+([0-9]+)");
        RegexMatchesPtr matches = regex.match(mem_total_line.c_str());

        if (matches) {
            NOVA_LOG_DEBUG("line : %s", mem_total_line.c_str());

            if (!matches->exists_at(2)) {
                throw InterrogatorException(
                    InterrogatorException::PATTERN_DOES_NOT_MATCH);
            }

            string key;
            string value;

            key = matches->get(1);
            value = matches->get(2);

            if (key == "MemTotal") {
                mem_total = boost::lexical_cast<int>(value);
            }

            NOVA_LOG_DEBUG("%s : %s", key.c_str(), value.c_str());
        }
    }
    meminfo_file.close();
    return mem_total;
}

int Interrogator::get_num_cpus() {
    string proc_cpuinfo_file = "/proc/cpuinfo";
    NOVA_LOG_DEBUG("getting cpu info from : %s", proc_cpuinfo_file.c_str());

    int num_cpus = 0;
    string processor_line;

    ifstream cpuinfo_file(proc_cpuinfo_file.c_str());
    if (!cpuinfo_file.is_open()) {
        throw InterrogatorException(InterrogatorException::FILE_NOT_FOUND);
    }
    while (cpuinfo_file.good()) {
        getline (cpuinfo_file,processor_line);

        Regex regex("(processor)\\s+:\\s+([0-9]+)");
        RegexMatchesPtr matches = regex.match(processor_line.c_str());

        if (matches) {
            NOVA_LOG_DEBUG("line : %s", processor_line.c_str());

            if (!matches->exists_at(2)) {
                throw InterrogatorException(
                    InterrogatorException::PATTERN_DOES_NOT_MATCH);
            }

            string key;
            string value;

            key = matches->get(1);
            value = matches->get(2);

            if (key == "processor") {
                num_cpus += 1;
            }

            NOVA_LOG_DEBUG("%s : %s", key.c_str(), value.c_str());
        }
    }
    cpuinfo_file.close();
    return num_cpus;
}

void Interrogator::get_proc_status(pid_t pid, ProcStatus & process_info) {
    string proc_status_file = str(format("/proc/%s/status") % pid);
    NOVA_LOG_DEBUG("proc status file location : %s",
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
            NOVA_LOG_DEBUG("line : %s", stat_line.c_str());

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

            NOVA_LOG_DEBUG("%s : %s", key.c_str(), value.c_str());
        }
    }
    status_file.close();
}

FileSystemStatsPtr Interrogator::get_filesystem_stats(std::string fs_path) {
    NOVA_LOG_DEBUG("Retrieving FileSystem Stats for : %s", fs_path.c_str());
    struct statvfs stats_buf;
    FileSystemStatsPtr fs_stats(new FileSystemStats());
    if (!statvfs(fs_path.c_str(), &stats_buf)) {
        fs_stats->block_size = stats_buf.f_bsize;
        fs_stats->total_blocks = stats_buf.f_blocks;
        fs_stats->free_blocks = stats_buf.f_bfree;
        unsigned long total_bytes =  fs_stats->total_blocks * fs_stats->block_size;
        unsigned long free_bytes = fs_stats->free_blocks * fs_stats->block_size;
        unsigned long used_bytes = total_bytes - free_bytes;
        // Convert used, free, total to GBs
        fs_stats->used = used_bytes / BYTES2GB;
        fs_stats->free = free_bytes / BYTES2GB;
        fs_stats->total = total_bytes / BYTES2GB;
        return fs_stats;
    } else {
        throw InterrogatorException(InterrogatorException::FILESYSTEM_NOT_FOUND);
    }
}

} } } // end namespace nova::guest::diagnostics
