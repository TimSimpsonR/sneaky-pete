#ifndef __NOVA_GUEST_BACKUP_BACKUPMANAGER_H
#define __NOVA_GUEST_BACKUP_BACKUPMANAGER_H

#include <boost/optional.hpp>
#include <boost/shared_ptr.hpp>
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

namespace nova { namespace guest { namespace backup {

    struct BackupInfo {
        const std::string backup_type;
        const std::string checksum;
        const std::string id;
        const std::string location;
    };


    class BackupManager {
        public:
            BackupManager(
                   nova::rpc::ResilientSenderPtr sender,
                   nova::utils::JobRunner & runner,
                   const nova::process::CommandList commands,
                   const int segment_max_size,
                   const std::string swift_container,
                   const double time_out,
                   const int zlib_buffer_size);

            ~BackupManager();

            void run_backup(const std::string & tenant,
                            const std::string & token,
                            const BackupInfo & backup_info);


        private:
            nova::rpc::ResilientSenderPtr sender;
            const nova::process::CommandList commands;
            nova::utils::JobRunner & runner;
            const int segment_max_size;
            const std::string swift_container;
            const std::string swift_url;
            const double time_out;
            const int zlib_buffer_size;
    };

} } }  // end namespace

#endif //__NOVA_GUEST_BACKUP_BACKUPMANAGER_H
