#ifndef __NOVA_REDIS_REDISBACKUP_H
#define __NOVA_REDIS_REDISBACKUP_H

#include "nova/backup/BackupManager.h"
#include "nova/backup/BackupRestore.h"
#include "nova/process.h"
#include "nova/utils/regex.h"
#include <boost/shared_ptr.hpp>
#include <string>


namespace nova { namespace redis {

    class RedisBackupManager : public nova::backup::BackupManager {
        public:
            RedisBackupManager(const nova::backup::BackupManagerInfo & info);

            virtual ~RedisBackupManager();

            template<typename Flags, typename... Args>
            static nova::backup::BackupManagerPtr from_flags(
                const Flags & flags, Args & ... args)
            {
                using nova::backup::BackupManagerInfo;
                using nova::backup::BackupManagerPtr;

                BackupManagerInfo info =
                    BackupManagerInfo::from_flags(flags, args...);
                BackupManagerPtr result;
                result.reset(new RedisBackupManager(info));
                return result;
            }

            virtual void run_backup(
                const nova::backup::BackupCreationArgs & args);
    };


    class RedisBackupRestoreManager : public nova::backup::BackupRestoreManager {
        public:
            RedisBackupRestoreManager(const nova::process::CommandList command_list,
                                      const size_t zlib_buffer_size);

            template<typename Flags>
            static nova::backup::BackupRestoreManagerPtr from_flags(
                const Flags & flags)
            {
                nova::backup::BackupRestoreManagerPtr ptr(
                    new RedisBackupRestoreManager(
                        flags.backup_restore_process_commands(),
                        flags.backup_restore_zlib_buffer_size()
                    ));
                return ptr;
            }

            virtual void run(const nova::backup::BackupRestoreInfo & restore);

            class Job;

        private:

            const nova::process::CommandList commands;
            const size_t zlib_buffer_size;
    };
} }

#endif
