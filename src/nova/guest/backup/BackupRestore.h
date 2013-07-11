#ifndef __NOVA_GUEST_BACKUP_BACKUPRESTORERUNNER_H
#define __NOVA_GUEST_BACKUP_BACKUPRESTORERUNNER_H

#include <string>

namespace nova { namespace guest { namespace backup {

    class BackupRestore {
        public:
            BackupRestore(const std::string & token,
                          const std::string & backup_url);

            void run();

        private:
            std::string backup_url;
            std::string token;
    };

} } }  // end namespace

#endif //__NOVA_GUEST_BACKUP_BACKUPRESTORERUNNER_H
