#ifndef __NOVA_GUEST_MYSQL_MYSQLMESSAGEHANDLER_H
#define __NOVA_GUEST_MYSQL_MYSQLMESSAGEHANDLER_H

#include "nova/guest/guest.h"
#include <map>
#include "nova/guest/mysql/MySqlAdmin.h"
#include "nova/guest/mysql/MySqlNovaUpdater.h"
#include "nova/guest/mysql/MySqlPreparer.h"
#include <string>


namespace nova { namespace guest { namespace mysql {

    struct MySqlMessageHandlerConfig {
        nova::guest::apt::AptGuest * apt;
        nova::guest::mysql::MySqlNovaUpdaterPtr sql_updater;
    };


    //TODO(tim.simpson): This should probably be called MySqlGuest.
    class MySqlMessageHandler : public MessageHandler {

        public:
            MySqlMessageHandler(MySqlMessageHandlerConfig config);

            virtual JsonDataPtr handle_message(const GuestInput & input);

            typedef nova::JsonDataPtr (* MethodPtr)(
                const MySqlMessageHandler *, nova::JsonObjectPtr);

            typedef std::map<std::string, MethodPtr> MethodMap;

            MySqlAdminPtr sql_admin() const;

            MySqlPreparerPtr sql_preparer() const;

            MySqlNovaUpdaterPtr sql_updater() const;

        private:
            MySqlMessageHandler(const MySqlMessageHandler & other);

            MySqlMessageHandlerConfig config;

            MethodMap methods;
    };

} } }

#endif
