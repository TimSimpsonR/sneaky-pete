#include "guest.h"
#include <boost/foreach.hpp>
#include <memory>
#include "sql_guest.h"
#include <sstream>
#include <uuid/uuid.h>

using sql::PreparedStatement;
using sql::ResultSet;
using sql::SQLException;
using namespace std;


/**---------------------------------------------------------------------------
 *- MySQLUser
 *---------------------------------------------------------------------------*/

MySQLUser::MySQLUser()
: name(""), password(""), databases("")
{
}

void MySQLUser::set_name(const std::string & value) {
    this->name = value;
}

void MySQLUser::set_password(const std::string & value) {
    this->password = value;
}

void MySQLUser::set_databases(const std::string & value) {
    this->databases = value;
}


/**---------------------------------------------------------------------------
 *- MySqlGuest
 *---------------------------------------------------------------------------*/

MySqlGuest::MySqlGuest()
:   driver(0), con(0)
{
    driver = get_driver_instance();
    con = driver->connect("tcp://127.0.0.1:3306", "root", "");
    try {
        // Connect to the MySQL test database
        con->setSchema("mysql");
    } catch (SQLException &e) {
        syslog(LOG_ERR, "Error with connection %s", e.what());
        delete con;
        throw e;
    }
}

MySqlGuest::~MySqlGuest() {
    delete con;
}

MySqlGuest::PreparedStatementPtr MySqlGuest::prepare_statement(const char * text) {
    auto_ptr<PreparedStatement> stmt(con->prepareStatement(text));
    return stmt;
}

string MySqlGuest::create_user(const string & username,const string & password,
                             const string & host) {
    PreparedStatementPtr stmt = prepare_statement(
        "insert into mysql.user (Host, User, Password) values (?, ?, password(?))");
    stmt->setString(1, host.c_str());
    stmt->setString(2, username.c_str());
    stmt->setString(3, password.c_str());
    stmt->executeUpdate();
    stmt->close();
    return "MySqlGuest::create_user method";
}

MySQLUserListPtr MySqlGuest::list_users() {
    MySQLUserListPtr users(new MySQLUserList());
    PreparedStatementPtr stmt = prepare_statement(
        "select User from mysql.user where host != 'localhost';");
    auto_ptr<ResultSet> res(stmt->executeQuery());
    while (res->next()) {
        MySQLUserPtr user(new MySQLUser());
        user->set_name(res->getString("User"));
        users->push_back(user);
    }
    res->close();
    stmt->close();
    return users;
}
string MySqlGuest::delete_user(const string & username) {
    PreparedStatementPtr stmt = prepare_statement("DROP USER `?`");
    stmt->setString(1, username);
    stmt->executeUpdate();
    stmt->close();
    return "MySqlGuest::delete_user method";
}
string MySqlGuest::create_database(const string & database_name, const string & character_set, const string & collate) {
    PreparedStatementPtr stmt = prepare_statement(
        "CREATE DATABASE IF NOT EXISTS `?` CHARACTER SET = ? COLLATE = ?;");
    stmt->setString(1, database_name);
    stmt->setString(2, character_set);
    stmt->setString(3, collate);
    stmt->executeUpdate();
    stmt->close();
    return "MySqlGuest::create_database method";
}
string MySqlGuest::list_databases() {
    return "MySqlGuest::list_databases method";
}

string MySqlGuest::delete_database(const string & database_name) {
    return "MySqlGuest::delete_database method";
}

string MySqlGuest::enable_root() {
    {
        PreparedStatementPtr stmt = prepare_statement(
            "insert into mysql.user (Host, User) values (?, ?)");
        stmt->setString(1, "%");
        stmt->setString(2, "root");
        stmt->executeUpdate();
        stmt->close();
    }

    uuid_t id;
    uuid_generate(id);
    char *buf = new char[37];
    uuid_unparse(id, buf);
    uuid_clear(id);

    {
        PreparedStatementPtr stmt = prepare_statement(
            "UPDATE mysql.user SET Password=PASSWORD(?) WHERE User='root'");
        stmt->setString(1, buf);
        stmt->executeUpdate();
        stmt->close();
    }

    {
        PreparedStatementPtr stmt = prepare_statement(
            "GRANT ALL PRIVILEGES ON *.* TO 'root'@'%' WITH GRANT OPTION;");
        stmt->executeUpdate();
        stmt->close();
    }
    return string(buf);
}

string MySqlGuest::disable_root() {
    uuid_t id;
    sql::PreparedStatement *stmt;
    uuid_generate(id);
    char *buf = new char[37];
    uuid_unparse(id, buf);
    uuid_clear(id);
    syslog(LOG_INFO, "generate %s", buf);
    {
        PreparedStatementPtr stmt = prepare_statement(
            "DELETE FROM mysql.user where User='root' and Host!='localhost'");
        stmt->executeUpdate();
        stmt->close();
    }

    {
        stmt = con->prepareStatement(
            "UPDATE mysql.user SET Password=PASSWORD(?) WHERE User='root'");
        stmt->setString(1, buf);
        stmt->executeUpdate();
        stmt->close();
    }
    return "Guest::disable_root method";
}

bool MySqlGuest::is_root_enabled() {
    sql::ResultSet *res;
    try {
        PreparedStatementPtr stmt = prepare_statement(
            "SELECT User FROM mysql.user where User = 'root' "
            "and host != 'localhost';");
        res = stmt->executeQuery();
        size_t row_count = res->rowsCount();
        res->close();
        stmt->close();
        delete res;
        return row_count != 0;
    } catch (sql::SQLException &e) {
        delete res;
        throw e;
    }
    return "Guest::is_root_enabled method";
}

/**---------------------------------------------------------------------------
 *- MySqlMessageHandler
 *---------------------------------------------------------------------------*/

#define STR_VALUE(arg)  #arg
#define METHOD_NAME(name) STR_VALUE(name)
#define REGISTER(name) { METHOD_NAME(name), & name }
#define JSON_METHOD(name) json_object * \
    name(const MySqlGuestPtr & guest, json_object * args)

namespace {

    JSON_METHOD(create_database) {
        syslog(LOG_INFO, "guest create_database"); //", guest->create_database().c_str());
        return json_object_new_object();
    }

    JSON_METHOD(create_user) {
        string guest_return = guest->create_user("username", "password", "%");
        syslog(LOG_INFO, "guest call %s", guest_return.c_str());
        return json_object_new_object();
    }

    JSON_METHOD(list_users) {
        std::stringstream user_xml;
        MySQLUserListPtr users = guest->list_users();
        user_xml << "[";
        BOOST_FOREACH(MySQLUserPtr & user, *users) {
            user_xml << "{'name':'" << user->get_name()
                     << "'},";
        }
        user_xml << "]";
        syslog(LOG_INFO, "guest call %s", user_xml.str().c_str());
        return json_object_new_object();
    }

    JSON_METHOD(delete_user) {
        string guest_return = guest->delete_user("username");
        syslog(LOG_INFO, "guest call %s", guest_return.c_str());
        return json_object_new_object();
    }

    JSON_METHOD(list_databases) {
        string guest_return = guest->list_databases();
        syslog(LOG_INFO, "guest call %s", guest_return.c_str());
        return json_object_new_object();
    }

    JSON_METHOD(delete_database) {
        string guest_return = guest->delete_database("database_name");
        syslog(LOG_INFO, "guest call %s", guest_return.c_str());
        return json_object_new_object();
    }

    JSON_METHOD(enable_root) {
        string guest_return = guest->enable_root();
        syslog(LOG_INFO, "roots new password %s", guest_return.c_str());
        return json_object_new_object();
    }

    JSON_METHOD(disable_root) {
        string guest_return = guest->disable_root();
        syslog(LOG_INFO, "guest call %s", guest_return.c_str());
        return json_object_new_object();
    }

    JSON_METHOD(is_root_enabled) {
        bool enabled = guest->is_root_enabled();
        syslog(LOG_INFO, "guest call %i", enabled);
        return json_object_new_object();
    }

    struct MethodEntry {
        const char * const name;
        const MySqlMessageHandler::MethodPtr ptr;
    };

}

MySqlMessageHandler::MySqlMessageHandler(MySqlGuestPtr guest)
:   guest(guest),
    methods()
{
    const MethodEntry static_method_entries [] = {
        REGISTER(create_database),
        REGISTER(create_user),
        REGISTER(delete_database),
        REGISTER(disable_root),
        REGISTER(delete_user),
        REGISTER(enable_root),
        REGISTER(is_root_enabled),
        REGISTER(list_databases),
        REGISTER(list_users),
        {0, 0}
    };
    const MethodEntry * itr = static_method_entries;
    while(itr->name != 0) {
        syslog(LOG_INFO, "Registering method %s", itr->name);
        methods[itr->name] = itr->ptr;
        itr ++;
    }
}

json_object * MySqlMessageHandler::handle_message(json_object * arguments) {
    json_object *method = json_object_object_get(arguments, "method");
    string method_name = json_object_to_json_string(method);

    MethodMap::iterator method_itr = methods.find(method_name);
    if (method_itr != methods.end()) {
        MethodPtr & method = method_itr->second;  // value
        return (*(method))(this->guest, arguments);
    } else {
        return 0;
    }
}
