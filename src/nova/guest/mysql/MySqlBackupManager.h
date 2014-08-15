#ifndef __NOVA_GUEST_BACKUP_MYSQLBACKUPMANAGER_H
#define __NOVA_GUEST_BACKUP_MYSQLBACKUPMANAGER_H

#include "nova/backup/BackupManager.h"

namespace nova { namespace guest { namespace mysql {

    /**
     * Handles MySql backups.
     */
    class MySqlBackupManager : public nova::backup::BackupManager {
        public:
            MySqlBackupManager(const nova::backup::BackupManagerInfo & info,
                               const nova::process::CommandList commands,
                               const int zlib_buffer_size);

            template<typename Flags, typename... Args>
            static nova::backup::BackupManagerPtr from_flags(
                const Flags & flags, Args & ... args)
            {
                using nova::backup::BackupManagerInfo;
                using nova::backup::BackupManagerPtr;

                BackupManagerInfo info =
                    BackupManagerInfo::from_flags(flags, args...);
                BackupManagerPtr result;
                result.reset(new MySqlBackupManager(
                  info,
                  flags.backup_process_commands(),
                  flags.backup_zlib_buffer_size()));
                return result;
            }

            ~MySqlBackupManager();

            virtual void run_backup(
                const nova::backup::BackupCreationArgs & args);

        private:
            const nova::process::CommandList commands;
            const int zlib_buffer_size;
    };

} } }  // end namespace

#endif //__NOVA_GUEST_BACKUP_MYSQLBACKUPMANAGER_H
