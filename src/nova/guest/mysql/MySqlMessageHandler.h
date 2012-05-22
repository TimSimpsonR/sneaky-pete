#ifndef __NOVA_GUEST_MYSQL_MYSQLMESSAGEHANDLER_H
#define __NOVA_GUEST_MYSQL_MYSQLMESSAGEHANDLER_H

#include "nova/guest/guest.h"
#include <map>
#include "nova/guest/mysql/MySqlAdmin.h"
#include "nova/guest/mysql/MySqlAppStatus.h"
#include "nova/guest/mysql/MySqlApp.h"
#include <string>


namespace nova { namespace guest { namespace mysql {


    //TODO(tim.simpson): This should probably be called MySqlGuest.
    class MySqlMessageHandler : public MessageHandler {

        public:
            MySqlMessageHandler();

            virtual JsonDataPtr handle_message(const GuestInput & input);

            typedef nova::JsonDataPtr (* MethodPtr)(
                const MySqlMessageHandler *, nova::JsonObjectPtr);

            typedef std::map<std::string, MethodPtr> MethodMap;

            nova::guest::apt::AptGuest & apt_guest() const;

            static MySqlAdminPtr sql_admin();

        private:
            MySqlMessageHandler(const MySqlMessageHandler & other);
            MySqlMessageHandler & operator = (const MySqlMessageHandler &);

            MethodMap methods;
    };

    class MySqlAppMessageHandler : public MessageHandler {

        public:
            MySqlAppMessageHandler(nova::guest::apt::AptGuest & apt,
                                   MySqlAppStatusPtr status,
                                   int state_change_wait_time);

            virtual ~MySqlAppMessageHandler();

            virtual JsonDataPtr handle_message(const GuestInput & input);

            MySqlAppPtr create_mysql_app();

        private:
            nova::guest::apt::AptGuest & apt;
            MySqlAppStatusPtr status;
            int state_change_wait_time;
    };

} } }

#endif
