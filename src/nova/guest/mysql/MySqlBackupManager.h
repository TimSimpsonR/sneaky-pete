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
                               const nova::process::CommandList commands);

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
                  info, flags.backup_process_commands()));
                return result;
            }

            ~MySqlBackupManager();

            virtual void run_backup(const std::string & tenant,
                                    const std::string & token,
                                    const nova::backup::BackupInfo & backup_info);

        private:
            const nova::process::CommandList commands;
    };

} } }  // end namespace

#endif //__NOVA_GUEST_BACKUP_MYSQLBACKUPMANAGER_H
