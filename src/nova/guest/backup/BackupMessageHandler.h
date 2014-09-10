#ifndef __NOVA_GUEST_BACKUP_BACKUPMESSAGEHANDLER_H
#define __NOVA_GUEST_BACKUP_BACKUPMESSAGEHANDLER_H

#include "nova/guest/guest.h"
#include "nova/backup/BackupManager.h"


namespace nova { namespace guest { namespace backup {

    class BackupMessageHandler : public nova::guest::MessageHandler {

        public:
          BackupMessageHandler(nova::backup::BackupManagerPtr backup_manager);

          virtual nova::JsonDataPtr handle_message(const GuestInput & input);

        private:
          BackupMessageHandler(const BackupMessageHandler &);
          BackupMessageHandler & operator = (const BackupMessageHandler &);

          nova::backup::BackupManagerPtr backup_manager;
    };

} } }  // end namespace

#endif //__NOVA_GUEST_BACKUP_BACKUPMESSAGEHANDLER_H
