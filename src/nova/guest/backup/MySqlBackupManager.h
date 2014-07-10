#ifndef __NOVA_GUEST_BACKUP_MYSQLBACKUPMANAGER_H
#define __NOVA_GUEST_BACKUP_MYSQLBACKUPMANAGER_H

#include "BackupManager.h"

namespace nova { namespace guest { namespace backup {

    /**
     * Handles MySql backups.
     */
    class MySqlBackupManager : public BackupManager {
        public:
            MySqlBackupManager(const BackupManagerInfo & info,
                               const nova::process::CommandList commands);

            template<typename Flags, typename... Args>
            static BackupManagerPtr create(const Flags & flags, Args & ... args) {
                BackupManagerInfo info
                    = BackupManagerInfo::create(flags, args...);
                BackupManagerPtr result;
                result.reset(new MySqlBackupManager(
                  info, flags.backup_process_commands()));
                return result;
            }

            ~MySqlBackupManager();

            virtual void run_backup(const std::string & tenant,
                                    const std::string & token,
                                    const BackupInfo & backup_info);

        private:
            const nova::process::CommandList commands;
    };

} } }  // end namespace

#endif //__NOVA_GUEST_BACKUP_MYSQLBACKUPMANAGER_H
