#ifndef __NOVA_GUEST_BACKUP_BACKUPMANAGER_H
#define __NOVA_GUEST_BACKUP_BACKUPMANAGER_H

#include <boost/optional.hpp>
#include "nova/db/mysql.h"
#include <boost/shared_ptr.hpp>
#include "nova/guest/guest.h"
#include "nova/process.h"
#include <map>
#include <string>
#include <curl/curl.h>
#include "nova/utils/swift.h"
#include "nova/utils/threads.h"
#include <boost/utility.hpp>

namespace nova { namespace guest { namespace backup {


    class BackupManager {
        public:
            BackupManager(
                   nova::db::mysql::MySqlConnectionWithDefaultDbPtr & infra_db,
                   nova::utils::JobRunner & runner,
                   const nova::process::CommandList commands,
                   const int segment_max_size,
                   const std::string swift_container,
                   const double time_out,
                   const int zlib_buffer_size);

            ~BackupManager();

            void run_backup(const std::string & swift_url,
                            const std::string & tenant,
                            const std::string & token,
                            const std::string & backup_id);


        private:
            nova::db::mysql::MySqlConnectionWithDefaultDbPtr infra_db;
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
