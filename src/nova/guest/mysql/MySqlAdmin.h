#ifndef __NOVA_GUEST_MYSQL_ADMIN_H
#define __NOVA_GUEST_MYSQL_ADMIN_H


#include "nova/db/mysql.h"
#include <boost/optional.hpp>
#include <boost/smart_ptr.hpp>
#include <string>
#include <boost/tuple/tuple.hpp>
#include "nova/guest/mysql/types.h"
#include <vector>


namespace nova { namespace guest { namespace mysql {

    std::string extract_user(const std::string & user);
    std::string generate_password();

    class MySqlAdmin {

        public:
            MySqlAdmin(nova::db::mysql::MySqlConnectionPtr con);

            ~MySqlAdmin();

            void create_database(MySqlDatabaseListPtr databases);

            void create_user(MySqlUserPtr, const char * host="%");

            void create_users(MySqlUserListPtr);

            void delete_database(const std::string & database_name);

            void delete_user(const std::string & username);

            MySqlUserPtr enable_root();

            inline nova::db::mysql::MySqlConnectionPtr get_connection() {
                return con;
            }

            boost::tuple<MySqlDatabaseListPtr, boost::optional<std::string> >
                list_databases(unsigned int limit,
                               boost::optional<std::string> marker);

            boost::tuple<MySqlUserListPtr, boost::optional<std::string> >
                list_users(unsigned int limit,
                           boost::optional<std::string> marker);

            bool is_root_enabled();

            void set_password(const char * username, const char * password);

        private:
            MySqlAdmin(const MySqlAdmin & other);
            MySqlAdmin & operator = (const MySqlAdmin &);

            nova::db::mysql::MySqlConnectionPtr con;

    };

    typedef boost::shared_ptr<MySqlAdmin> MySqlAdminPtr;

} } }  // end namespace

#endif //__NOVA_GUEST_SQL_GUEST_H
