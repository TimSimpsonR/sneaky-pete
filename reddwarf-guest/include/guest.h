#ifndef __NOVAGUEST
#define __NOVAGUEST
#include <cstdlib>
#include <cstdio>
#include <json/json.h>
#include <string>
#include <memory>
#include <syslog.h>
#include <unistd.h>
#include <vector>
#include <mysql_connection.h>
#include <boost/smart_ptr.hpp>
#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>
#include <cppconn/prepared_statement.h>
#include <vector>


class MySQLUser {
public:
    MySQLUser();

    inline const std::string & get_name() const {
        return name;
    }

    inline const std::string & get_password() const {
        return password;
    }

    inline const std::string & get_databases() const {
        return databases;
    }

    void set_name(const std::string & value);

    void set_password(const std::string & value);

    void set_databases(const std::string & value);

private:
    std::string name;
    std::string password;
    std::string databases;
};

typedef boost::shared_ptr<MySQLUser> MySQLUserPtr;
typedef std::vector<MySQLUserPtr> MySQLUserList;
typedef boost::shared_ptr<MySQLUserList> MySQLUserListPtr;

class Guest;

class MessageHandler {

public:
    MessageHandler();

    void handle_message(json_object * json);

private:
    std::auto_ptr<Guest> guest;
};

class Guest {

    typedef std::auto_ptr<sql::PreparedStatement> PreparedStatementPtr;

    sql::Driver *driver;
    sql::Connection *con;


    public:
        Guest();
        ~Guest();

        std::string create_user(const std::string & username, const std::string & password, const std::string & host);
        // You must delete the users that come from this list using something like `vector.clear();`
        MySQLUserListPtr list_users();
        std::string delete_user(const std::string & username);
        std::string create_database(const std::string & database_name, const std::string & character_set, const std::string & collate);
        std::string list_databases();
        std::string delete_database(const std::string & database_name);
        std::string enable_root();
        std::string disable_root();
        bool is_root_enabled();
        PreparedStatementPtr prepare_statement(const char * text);
};

#endif //__NOVAGUEST
