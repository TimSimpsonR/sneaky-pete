#ifndef __NOVAGUEST
#define __NOVAGUEST
#include <cstdlib>
#include <cstdio>
#include <string>
#include <syslog.h>
#include <unistd.h>
#include <mysql_connection.h>
#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>
#include <cppconn/prepared_statement.h>


class MySQLUser {
    public:
        std::string name;
        std::string password;
        std::string databases;
};


class Guest {
    sql::Driver *driver;
    sql::Connection *con;

    public:
        Guest();
        ~Guest();
        std::string create_user(const std::string & username, const std::string & password, const std::string & host);
        // You must delete the users that come from this list using something like `vector.clear();`
        std::vector<MySQLUser> list_users();
        std::string delete_user(const std::string & username);
        std::string create_database(const std::string & database_name, const std::string & character_set, const std::string & collate);
        std::string list_databases();
        std::string delete_database(const std::string & database_name);
        std::string enable_root();
        std::string disable_root();
        bool is_root_enabled();
};

#endif //__NOVAGUEST
