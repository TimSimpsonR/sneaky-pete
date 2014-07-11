#ifndef __NOVA_GUEST_BACKUP_BACKUPMANAGER_H
#define __NOVA_GUEST_BACKUP_BACKUPMANAGER_H

#include <boost/optional.hpp>
#include <boost/shared_ptr.hpp>
#include "nova/guest/diagnostics.h"
#include "nova/guest/guest.h"
#include "nova/process.h"
#include <map>
#include <string>
#include <curl/curl.h>
#include "nova/utils/swift.h"
#include "nova/utils/threads.h"
#include <boost/utility.hpp>
#include "nova/rpc/sender.h"
#include "nova/utils/subsecond.h"

namespace nova { namespace backup {

    struct BackupInfo {
        const std::string backup_type;
        const std::string checksum;
        const std::string id;
        const std::string location;
    };

    /**
     * A common subclass for all the backup handlers which also serves as a
     * collection of config values and other things. Safe to copy which is
     * necessary to implement the subclasses without duplicating a ton of code.
     */
    class BackupManagerInfo {
        public:
            BackupManagerInfo(
                   nova::rpc::ResilientSenderPtr sender,
                   nova::utils::JobRunner & runner,
                   const nova::guest::diagnostics::Interrogator interrogator,
                   const int segment_max_size,
                   const int checksum_wait_time,
                   const std::string swift_container,
                   const double time_out,
                   const int zlib_buffer_size);

            BackupManagerInfo(const BackupManagerInfo & info);

            template<typename Flags>
            static BackupManagerInfo from_flags(
                const Flags & flags,
                nova::rpc::ResilientSenderPtr sender,
                nova::utils::JobRunner & runner,
                const nova::guest::diagnostics::Interrogator interrogator) {
                BackupManagerInfo backup(
                      sender,
                      runner,
                      interrogator,
                      flags.backup_segment_max_size(),
                      flags.checksum_wait_time(),
                      flags.backup_swift_container(),
                      flags.backup_timeout(),
                      flags.backup_zlib_buffer_size());
                return backup;
            }

            virtual ~BackupManagerInfo();

        protected:
            nova::rpc::ResilientSenderPtr sender;
            nova::utils::JobRunner & runner;
            const nova::guest::diagnostics::Interrogator interrogator;
            const int segment_max_size;
            const int checksum_wait_time;
            const std::string swift_container;
            const std::string swift_url;
            const double time_out;
            const int zlib_buffer_size;
    };

    /**
     *  The root of the backup hierarchy which can be used with the
     *  message handler.
     */
    class BackupManager : public BackupManagerInfo {
        public:
            BackupManager(const BackupManagerInfo & info);
            virtual void run_backup(const std::string & tenant,
                                    const std::string & token,
                                    const BackupInfo & backup_info) = 0;
    };

    typedef boost::shared_ptr<BackupManager> BackupManagerPtr;

} }  // end namespace

#endif //__NOVA_GUEST_BACKUP_BACKUPMANAGER_H
