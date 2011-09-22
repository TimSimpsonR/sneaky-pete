#include "guest.h"
#include <boost/foreach.hpp>
#include <memory>
#include "sql_guest.h"
#include <sstream>
#include <regex.h>
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
 *- MySQLDatabase
 *---------------------------------------------------------------------------*/

MySQLDatabase::MySQLDatabase()
: name(""), collation(""), charset("")
{
}

void MySQLDatabase::set_name(const std::string & value) {
    this->name = value;
}

void MySQLDatabase::set_collation(const std::string & value) {
    this->collation = value;
}

void MySQLDatabase::set_charset(const std::string & value) {
    this->charset = value;
}


/**---------------------------------------------------------------------------
 *- MySqlGuest
 *---------------------------------------------------------------------------*/

MySqlGuest::MySqlGuest(const std::string & uri)
:   driver(0), con(0)
{
    string user;
    string password;
    
    get_username_and_password_from_config_file(user, password);
    syslog(LOG_INFO, "got these back %s, %s", user.c_str(), password.c_str());
    driver = get_driver_instance();
    con = driver->connect(uri, user, password);
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

void MySqlGuest::get_username_and_password_from_config_file(string &user, string &password) {
    const char *pattern = "^\\w+\\s*=\\s*(.*)$";
    regex_t regex;
    // 2 matches, the 2nd is the ()
    int num_matches = 2;
    regmatch_t matches[num_matches];
    regcomp(&regex, pattern, REG_EXTENDED);
    
    FILE *fp=fopen("/etc/mysql/my.cnf","r");
    char  tmp[256]={0x0};
    bool is_in_client = false;
    while(fp!=NULL && fgets(tmp, sizeof(tmp),fp) != NULL)
    {
        if (strstr(tmp, "[client]")) {
            is_in_client = true;
        }
        if (strstr(tmp, "[mysqld]")) {
            is_in_client = false;
        }
        
        if (is_in_client && strstr(tmp, "user")) {
            regexec(&regex, tmp, num_matches, matches, 0);
            const char* begin_of_new_string = tmp+matches[num_matches-1].rm_so;
            size_t match_size = matches[num_matches-1].rm_eo - matches[num_matches-1].rm_so - 1;
            user.append(begin_of_new_string, match_size);
        }
        if (is_in_client && strstr(tmp, "password")) {
            regexec(&regex, tmp, num_matches, matches, 0);
            const char* begin_of_new_string = tmp+matches[num_matches-1].rm_so;
            size_t match_size = matches[num_matches-1].rm_eo - matches[num_matches-1].rm_so - 1;
            password.append(begin_of_new_string, match_size);
        }
    }
    if(fp!=NULL) fclose(fp);
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
MySQLDatabaseListPtr MySqlGuest::list_databases() {
    MySQLDatabaseListPtr databases(new MySQLDatabaseList());
    PreparedStatementPtr stmt = prepare_statement("SELECT"
    "                schema_name as name,"
    "                default_character_set_name as charset,"
    "                default_collation_name as collation"
    "            FROM"
    "                information_schema.schemata"
    "            WHERE"
    "                schema_name not in"
    "                ('mysql', 'information_schema', 'lost+found')"
    "            ORDER BY"
    "                schema_name ASC;");

    auto_ptr<ResultSet> res(stmt->executeQuery());
    while (res->next()) {
        MySQLDatabasePtr database(new MySQLDatabase());
        database->set_name(res->getString("name"));
        database->set_collation(res->getString("collation"));
        database->set_charset(res->getString("charset"));
        databases->push_back(database);
    }
    res->close();
    stmt->close();
    return databases;
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
        PreparedStatementPtr stmt = prepare_statement(
            "UPDATE mysql.user SET Password=PASSWORD(?) WHERE User='root'");
        stmt->setString(1, buf);
        stmt->executeUpdate();
        stmt->close();
    }
    return "Guest::disable_root method";
}

bool MySqlGuest::is_root_enabled() {
    PreparedStatementPtr stmt = prepare_statement(
        "SELECT User FROM mysql.user where User = 'root' "
        "and host != 'localhost';");
    auto_ptr<ResultSet> res(stmt->executeQuery());
    size_t row_count = res->rowsCount();
    res->close();
    stmt->close();
    return row_count != 0;
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
        std::stringstream database_xml;
        MySQLDatabaseListPtr databases = guest->list_databases();
        database_xml << "[";
        BOOST_FOREACH(MySQLDatabasePtr & database, *databases) {
            database_xml << "{'name':'" << database->get_name()
                         << "', 'collation':'" << database->get_collation()
                         << "', 'charset':'" << database->get_charset()
                     << "'},";
        }
        database_xml << "]";
        syslog(LOG_INFO, "guest call %s", database_xml.str().c_str());
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
    string method_name = json_object_get_string(method);

    MethodMap::iterator method_itr = methods.find(method_name);
    if (method_itr != methods.end()) {
        MethodPtr & method = method_itr->second;  // value
        syslog(LOG_INFO, "Executing method %s", method_name.c_str());
        return (*(method))(this->guest, arguments);
    } else {
        syslog(LOG_INFO, "Couldnt find a method for %s", method_name.c_str());
        return 0;
    }
}
