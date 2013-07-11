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
    std::string extract_host(const std::string & user);
    std::string generate_password();

    class MySqlAdmin {

        public:
            MySqlAdmin(nova::db::mysql::MySqlConnectionPtr con);

            ~MySqlAdmin();

            void change_passwords(MySqlUserListPtr);

            void update_attributes(const std::string & username, const std::string & hostname, MySqlUserAttrPtr);

            void create_database(MySqlDatabaseListPtr databases);

            void create_user(MySqlUserPtr);

            void create_users(MySqlUserListPtr);

            void delete_database(const std::string & database_name);

            void delete_user(const std::string & username, const std::string & hostname);

            MySqlUserPtr enable_root();

            MySqlUserPtr find_user(const std::string & username, const std::string & hostname);

            inline nova::db::mysql::MySqlConnectionPtr get_connection() {
                return con;
            }

            void grant_access(const std::string & user_name, const std::string & host_name, MySqlDatabaseListPtr databases);

            boost::tuple<MySqlDatabaseListPtr, boost::optional<std::string> >
                list_databases(unsigned int limit,
                               boost::optional<std::string> marker,
                               bool include_marker = false);

            boost::tuple<MySqlUserListPtr, boost::optional<std::string> >
                list_users(unsigned int limit,
                           boost::optional<std::string> marker,
                           bool include_marker = false);

            bool is_root_enabled();

            MySqlDatabaseListPtr list_access(const std::string & user_name, const std::string & host_name);

            void revoke_access(const std::string & user_name, const std::string & host_name, const std::string & database_name);

            void set_password(const char * username, const char * hostname, const char * password);

        private:
            MySqlAdmin(const MySqlAdmin & other);
            MySqlAdmin & operator = (const MySqlAdmin &);

            nova::db::mysql::MySqlConnectionPtr con;

    };

    typedef boost::shared_ptr<MySqlAdmin> MySqlAdminPtr;

} } }  // end namespace

#endif //__NOVA_GUEST_SQL_GUEST_H
