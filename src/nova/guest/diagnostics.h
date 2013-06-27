#ifndef __NOVA_GUEST_DIAGNOSTICS_H
#define __NOVA_GUEST_DIAGNOSTICS_H

#include <boost/optional.hpp>
#include <boost/shared_ptr.hpp>
#include "nova/guest/guest.h"
#include <map>
#include <string>

namespace nova { namespace guest { namespace diagnostics {

    struct ProcStatus {
        int fd_size;
        int vm_size;
        int vm_peak;
        int vm_rss;
        int vm_hwm;
        int threads;

        ProcStatus operator - (const ProcStatus & rhs) const;
    };

    struct DiagInfo : public ProcStatus {
        std::string version;
    };


    class DiagnosticsMessageHandler : public MessageHandler {
        public:
          DiagnosticsMessageHandler(bool enabled);

          virtual nova::JsonDataPtr handle_message(const GuestInput & input);

        private:
          DiagnosticsMessageHandler(const DiagnosticsMessageHandler &);
          DiagnosticsMessageHandler & operator = (const DiagnosticsMessageHandler &);

          bool enabled;
    };


    struct FileSystemStats {
        unsigned long block_size;
        unsigned long total_blocks;
        unsigned long free_blocks;
        unsigned long total;
        unsigned long free;
        unsigned long used;
    };

    struct HwInfo {
        int mem_total;
        int num_cpus;
    };

    typedef std::auto_ptr<const HwInfo> HwInfoPtr;
    typedef std::auto_ptr<const DiagInfo> DiagInfoPtr;
    typedef boost::shared_ptr<FileSystemStats> FileSystemStatsPtr;

    class Interrogator {
        public:
            /** Grabs diagnostics for this program. */
            static DiagInfoPtr get_diagnostics();

            /** Grabs process status for the given process id. */
            static void get_proc_status(pid_t pid, ProcStatus & status);

            /** Retrieves the File System stats for a given volume/device. */
            static FileSystemStatsPtr get_filesystem_stats(std::string fs);

            /** Get hardware information */
            static HwInfoPtr get_hwinfo();

            /** Get the total memory from /proc/meminfo. */
            static int get_mem_total();

            /** Get the total number of CPUs from /proc/cpuinfo. */
            static int get_num_cpus();
    };

    class InterrogatorException : public std::exception {

        public:
            enum Code {
                FILE_NOT_FOUND,
                PATTERN_DOES_NOT_MATCH,
                FILESYSTEM_NOT_FOUND
            };

            InterrogatorException(const Code code) throw();

            virtual ~InterrogatorException() throw();

            virtual const char * what() const throw();

            const Code code;

    };


    class InterrogatorMessageHandler : public MessageHandler {

        public:
          InterrogatorMessageHandler(const Interrogator & interrogator);

          virtual nova::JsonDataPtr handle_message(const GuestInput & input);

        private:
          InterrogatorMessageHandler(const InterrogatorMessageHandler &);
          InterrogatorMessageHandler & operator = (const InterrogatorMessageHandler &);

          const Interrogator & interrogator;
    };


} } }  // end namespace

#endif //__NOVA_GUEST_DIAGNOSTICS_H
