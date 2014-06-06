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

    const float BYTES2GB = 1024.0*1024.0*1024.0;

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
        float total;
        float free;
        float used;
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
            Interrogator(const std::string mount_point);

            Interrogator(const Interrogator & other);

            /** Grabs diagnostics for this program. */
            DiagInfoPtr get_diagnostics() const;

            /** Grabs process status for the given process id. */
            void get_proc_status(const pid_t pid, ProcStatus & status) const;

            /** Retrieves the File System stats for a given volume/device. */
            FileSystemStatsPtr get_filesystem_stats(const std::string & fs) const;

            /** Get hardware information */
            HwInfoPtr get_hwinfo() const;

            /** Get the total memory from /proc/meminfo. */
            int get_mem_total() const;

            /* Returns file system stats for the mount point. */
            FileSystemStatsPtr get_mount_point_stats() const;

            /** Get the total number of CPUs from /proc/cpuinfo. */
            int get_num_cpus() const;

        private:
            const std::string mount_point;
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

          const Interrogator interrogator;
    };


} } }  // end namespace

#endif //__NOVA_GUEST_DIAGNOSTICS_H
