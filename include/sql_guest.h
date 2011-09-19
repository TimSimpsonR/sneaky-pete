#ifndef __NOVAGUEST_SQL_GUEST_H
#define __NOVAGUEST_SQL_GUEST_H

#include "guest.h"

#include <cstdlib>
#include <cstdio>
#include <memory>
#include <string>
#include <syslog.h>
#include <unistd.h>
#include <vector>

#include <boost/smart_ptr.hpp>
#include <mysql_connection.h>
#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>
#include <cppconn/prepared_statement.h>
#include <json/json.h>
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

class MySqlGuest {

    public:
        MySqlGuest();
        ~MySqlGuest();

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

    private:
        typedef std::auto_ptr<sql::PreparedStatement> PreparedStatementPtr;

        sql::Driver *driver;
        sql::Connection *con;

        PreparedStatementPtr prepare_statement(const char * text);
};

typedef boost::shared_ptr<MySqlGuest> MySqlGuestPtr;

class MySqlMessageHandler : public MessageHandler {

    public:
        MySqlMessageHandler(MySqlGuestPtr guest);

        virtual json_object * handle_message(json_object * json);


        typedef json_object * (* MethodPtr)(const MySqlGuestPtr &,
                                            json_object *);
        typedef std::map<std::string, MethodPtr> MethodMap;

    private:

        MySqlGuestPtr guest;
        MethodMap methods;
};


#endif //__NOVAGUEST_SQL_GUEST_H
