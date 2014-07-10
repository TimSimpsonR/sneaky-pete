#ifndef __NOVA_GUEST_BACKUP_MYSQLBACKUPRESTORERUNNER_H
#define __NOVA_GUEST_BACKUP_MYSQLBACKUPRESTORERUNNER_H

#include "BackupRestore.h"
#include "nova/process.h"
#include "nova/utils/regex.h"
#include <boost/shared_ptr.hpp>
#include <string>


namespace nova { namespace guest { namespace backup {

    class MySqlBackupRestoreManager : public BackupRestoreManager {
        public:
            MySqlBackupRestoreManager(const nova::process::CommandList command_list,
                                 const std::string & delete_file_pattern,
                                 const std::string & restore_directory,
                                 const std::string & save_file_pattern,
                                 const size_t zlib_buffer_size);

            template<typename Flags>
            static BackupRestoreManagerPtr create(const Flags & flags) {
                BackupRestoreManagerPtr ptr(
                    new MySqlBackupRestoreManager(
                        flags.backup_restore_process_commands(),
                        flags.backup_restore_delete_file_pattern(),
                        flags.backup_restore_restore_directory(),
                        flags.backup_restore_save_file_pattern(),
                        flags.backup_restore_zlib_buffer_size()
                    ));
                return ptr;
            }

            virtual void run(const BackupRestoreInfo & restore);

        private:
            class BackupRestoreJob;

            const nova::process::CommandList commands;
            const nova::utils::Regex delete_file_pattern;
            const std::string restore_directory;
            const nova::utils::Regex save_file_pattern;
            const size_t zlib_buffer_size;
    };

} } }  // end namespace

#endif //__NOVA_GUEST_BACKUP_BACKUPRESTORERUNNER_H
