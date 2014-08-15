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


    // The arguments passed by Trove to the guest agent to perform a backup.
    struct BackupCreationArgs {
        const std::string tenant;
        const std::string token;
        const std::string id;           // Backup ID
        const std::string location;     // Swift URL.
    };

    // Arguments passed back to Trove as the guest updates the backup.
    struct BackupUpdateArgs {
        const std::string backup_type;  // Type of backu
        const std::string checksum;     // Checksum
        const std::string id;           // Backup Id
        const std::string location;     // This *should* be discovered via the
                                        // guest agent. For now though Trove
                                        // is passing it to the guest. :/
                                        // However, it seems to be a base URL
                                        // that could be used for multiple
                                        // backups. ????!!
        const float size;               // Size of the datastore on disk.
        const std::string state;        // String state. Strrrrrinnnngggs....
    };



    /** Contains data which is pretty much global and used by most backup
     *  job runners. */
    struct BackupRunnerData {
        nova::rpc::ResilientSenderPtr sender;
        const nova::guest::diagnostics::Interrogator interrogator;
        const int segment_max_size;
        const int checksum_wait_time;
        const std::string swift_container;
        const double time_out;

        template<typename Flags>
        static BackupRunnerData from_flags(
            const Flags & flags,
            nova::rpc::ResilientSenderPtr sender,
            const nova::guest::diagnostics::Interrogator interrogator) {
            const BackupRunnerData info = {
                sender,
                interrogator,
                flags.backup_segment_max_size(),
                flags.checksum_wait_time(),
                flags.backup_swift_container(),
                flags.backup_timeout()
            };
            return info;
        }
    };

    class BackupJob : public nova::utils::Job {
        public:
            BackupJob(const BackupRunnerData & data,
                      const BackupCreationArgs & args);

            ~BackupJob();

        protected:
            const BackupCreationArgs args;
            const BackupRunnerData data;
            const nova::utils::swift::SwiftFileInfo file_info;

            /** The type of the backup, represented by this job. */
            virtual const char * get_backup_type() const = 0;

            void update_trove_to_building();

            void update_trove_to_failed();

            void update_trove_to_completed(const std::string & checksum);

        private:
            enum Status {
                //! BEGIN GENERATED CODE
                COMPLETED,
                BUILDING,
                FAILED
                //! END GENERATED CODE
            };

            const char * status_name(Status status);

            void update_trove(const Status status,
                              const boost::optional<std::string> & checksum);

    };

    /**
     * A common subclass for all the backup handlers which also serves as a
     * collection of config values and other things. Safe to copy which is
     * necessary to implement the subclasses without duplicating a ton of code.
     */
    class BackupManagerInfo {
        public:
            BackupManagerInfo(
                   BackupRunnerData data,
                   nova::utils::JobRunner & runner);

            BackupManagerInfo(const BackupManagerInfo & info);

            template<typename Flags>
            static BackupManagerInfo from_flags(
                const Flags & flags,
                nova::rpc::ResilientSenderPtr sender,
                nova::utils::JobRunner & runner,
                const nova::guest::diagnostics::Interrogator interrogator) {
                BackupManagerInfo backup(
                      BackupRunnerData::from_flags(flags, sender, interrogator),
                      runner);
                return backup;
            }

            virtual ~BackupManagerInfo();

        protected:
            BackupRunnerData data;
            nova::utils::JobRunner & runner;
    };

    /**
     *  The root of the backup hierarchy which can be used with the
     *  message handler.
     */
    class BackupManager : public BackupManagerInfo {
        public:
            BackupManager(const BackupManagerInfo & info);
            virtual void run_backup(const BackupCreationArgs & args) = 0;
    };

    typedef boost::shared_ptr<BackupManager> BackupManagerPtr;

} }  // end namespace

#endif //__NOVA_GUEST_BACKUP_BACKUPMANAGER_H
