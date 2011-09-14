#ifndef __NOVAGUEST
#define __NOVAGUEST
#include <cstdlib>
#include <cstdio>
#include <string>
#include <syslog.h>
#include <unistd.h>
#include "json.h"
#include "mysql_connection.h"
#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>
#include <cppconn/prepared_statement.h>


class Guest {
    sql::Driver *driver;
    sql::Connection *con;
    
    public:
        Guest();
        ~Guest();
        std::string create_user(const std::string & username, const std::string & password, const std::string & host);
        std::string list_users();
        std::string delete_user(const std::string & username);
        std::string create_database(const std::string & database_name, const std::string & character_set, const std::string & collate);
        std::string list_databases();
        std::string delete_database(const std::string & database_name);
        std::string enable_root();
        std::string disable_root();
        std::string is_root_enabled();
};

class MySQLUser {
    std::string name;
    std::string password;
    std::string databases;
};

#endif //__NOVAGUEST
