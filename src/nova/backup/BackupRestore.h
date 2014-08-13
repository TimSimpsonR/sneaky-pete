#ifndef __NOVA_GUEST_BACKUP_BACKUPRESTORERUNNER_H
#define __NOVA_GUEST_BACKUP_BACKUPRESTORERUNNER_H

#include "nova/process.h"
#include "nova/utils/regex.h"
#include <boost/shared_ptr.hpp>
#include <string>


namespace nova { namespace backup {

    class BackupRestoreInfo {
        public:
            BackupRestoreInfo(const std::string & token,
                          const std::string & location,
                          const std::string & checksum);

            const std::string & get_backup_url() const {
                return location;
            }

            const std::string & get_token() const {
                return token;
            }

            const std::string & get_backup_checksum() const {
                return checksum;
            }

        private:
            std::string token;
            std::string location;
            std::string checksum;
    };

    class BackupRestoreManager {
        public:
            BackupRestoreManager();

            virtual ~BackupRestoreManager();

            virtual void run(const BackupRestoreInfo & restore) = 0;
    };

    typedef boost::shared_ptr<BackupRestoreManager> BackupRestoreManagerPtr;


    class BackupRestoreException  : public std::exception {
        public:
            virtual const char * what() const throw();
    };

} } // end namespace

#endif //__NOVA_GUEST_BACKUP_BACKUPRESTORERUNNER_H
