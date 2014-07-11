#ifndef __NOVA_GUEST_BACKUP_BACKUPEXCEPTION_H
#define __NOVA_GUEST_BACKUP_BACKUPEXCEPTION_H

#include <exception>

namespace nova { namespace backup {

    class BackupException : public std::exception {

        public:
            enum Code {
                INVALID_STATE
            };

            BackupException(const Code code) throw();

            virtual ~BackupException() throw();

            virtual const char * what() const throw();

            const Code code;
    };

} }  // end namespace

#endif //__NOVA_GUEST_BACKUP_BACKUPEXCEPTION_H
