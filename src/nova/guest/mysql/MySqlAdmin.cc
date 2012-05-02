#include "nova/guest/guest.h"
#include "nova/Log.h"
#include <boost/foreach.hpp>
#include <boost/format.hpp>

#include <fstream>
#include <iostream>
#include <memory>
//#include <mysql/mysql.h>
#include "nova/guest/mysql/MySqlAdmin.h"
#include "nova/guest/mysql/MySqlGuestException.h"
#include "nova/utils/regex.h"
#include <sstream>
#include <uuid/uuid.h>


using boost::format;
using nova::db::mysql::MySqlConnection;
using nova::db::mysql::MySqlConnectionPtr;
using nova::db::mysql::MySqlException;
using nova::db::mysql::MySqlResultSet;
using nova::db::mysql::MySqlResultSetPtr;
using nova::db::mysql::MySqlPreparedStatementPtr;
using boost::none;
using boost::optional;
using nova::Log;
using nova::utils::Regex;
using nova::utils::RegexMatchesPtr;
using namespace std;


namespace nova { namespace guest { namespace mysql {

namespace {
    typedef map<string, MySqlUserPtr> UserMap;
}

string extract_user(const string & user) {
    Regex regex("^'(.+)'@");
    RegexMatchesPtr matches = regex.match(user.c_str());
    if (matches || matches->exists_at(1)) {
        return matches->get(1);
    } else {
        NOVA_LOG_ERROR2("Failed to parse the grantee: %s", user.c_str());
        throw MySqlGuestException(MySqlGuestException::GENERAL);
    }
}

string generate_password() {
    uuid_t id;
    uuid_generate(id);
    char *buf = new char[37];
    uuid_unparse(id, buf);
    uuid_clear(id);
    return string(buf);
}

/**---------------------------------------------------------------------------
 *- MySqlAdmin
 *---------------------------------------------------------------------------*/

MySqlAdmin::MySqlAdmin(MySqlConnectionPtr con)
: con(con)
{
}

MySqlAdmin::~MySqlAdmin() {
}

void MySqlAdmin::create_database(MySqlDatabaseListPtr databases) {
    BOOST_FOREACH(MySqlDatabasePtr & db, *databases) {
        string text = str(format(
            "CREATE DATABASE IF NOT EXISTS `%s` CHARACTER SET = `%s` "
            "COLLATE = `%s`")
            % con->escape_string(db->get_name().c_str())
            % con->escape_string(db->get_character_set().c_str())
            % con->escape_string(db->get_collation().c_str()));
        con->query(text.c_str());
        //stmt->setString(1, db->get_name());
        //stmt->setString(2, db->get_character_set());
        //stmt->setString(3, db->get_collation());
        //stmt->execute();
        //stmt->close();
    }
}

void MySqlAdmin::create_user(MySqlUserPtr user, const char * host) {
    if (!user->get_password()) {
        throw MySqlGuestException(MySqlGuestException::NO_PASSWORD_FOR_CREATE_USER);
    }
    string text = str(format(
        "GRANT USAGE ON *.* TO '%s'@\"%s\" IDENTIFIED BY '%s'")
        //"CREATE USER `%s`@`%s` IDENTIFIED BY '%s'")
        % con->escape_string(user->get_name().c_str())
        % con->escape_string(host)
        % con->escape_string(user->get_password().get().c_str()));
    con->query(text.c_str());

    BOOST_FOREACH(MySqlDatabasePtr db, *user->get_databases()) {
        string text = str(format(
            "GRANT ALL PRIVILEGES ON `%s`.* TO `%s`@'%s'")
            % con->escape_string(db->get_name().c_str())
            % con->escape_string(user->get_name().c_str())
            % con->escape_string(host));
        con->query(text.c_str());
    }
}

void MySqlAdmin::create_users(MySqlUserListPtr users) {
    BOOST_FOREACH(MySqlUserPtr & user, *users) {
        create_user(user);
    }
}

void MySqlAdmin::delete_database(const string & database_name) {
    string text = str(format("DROP DATABASE IF EXISTS `%s`")
                       % con->escape_string(database_name.c_str()));
    con->query(text.c_str());
    con->flush_privileges();
}

void MySqlAdmin::delete_user(const string & username) {
    string text = str(format("DROP USER `%s`")
                       % con->escape_string(username.c_str()));
    con->query(text.c_str());
    con->flush_privileges();
}

MySqlUserPtr MySqlAdmin::enable_root() {
    MySqlUserPtr root_user(new MySqlUser());
    root_user->set_name("root");
    root_user->set_password(generate_password());
    try {
        create_user(root_user);
    } catch(const MySqlException & mse) {
        // Ignore, user is already created. We just have to reset the password.
    }
    set_password("root", root_user->get_password().get().c_str());
    con->grant_all_privileges("root", "%");
    con->revoke_privileges("root", "%", "FILE");
    con->flush_privileges();
    return root_user;
}

MySqlDatabaseListPtr MySqlAdmin::list_databases() {
    MySqlDatabaseListPtr databases(new MySqlDatabaseList());
    MySqlResultSetPtr res = con->query(
    "            SELECT"
    "                schema_name as name,"
    "                default_character_set_name as charset,"
    "                default_collation_name as collation"
    "            FROM"
    "                information_schema.schemata"
    "            WHERE"
    "                schema_name not in"
    "                ('mysql', 'information_schema', 'lost+found')"
    "            ORDER BY"
    "                schema_name ASC");

    while (res->next()) {
        MySqlDatabasePtr database(new MySqlDatabase());
        database->set_name(res->get_string(0).get());
        database->set_character_set(res->get_string(1).get());
        database->set_collation(res->get_string(2).get());
        databases->push_back(database);
    }
    res->close();
    return databases;
}

MySqlUserListPtr MySqlAdmin::list_users() {
    MySqlUserListPtr users(new MySqlUserList());
    UserMap user_map;
    // Get the list of users
    MySqlResultSetPtr res = con->query(
        "select User from mysql.user where host != 'localhost'");
    while (res->next()) {
        MySqlUserPtr user(new MySqlUser());
        user->set_name(res->get_string(0).get());
        users->push_back(user);
        user_map[user->get_name()] = user;
    }
    res->close();

    // Get the list of users and their associated databases
    MySqlResultSetPtr userDbRes = con->query(
                "SELECT grantee, table_schema from information_schema.SCHEMA_PRIVILEGES "
                "group by grantee, table_schema;");
    while(userDbRes->next()) {
        string username = extract_user(userDbRes->get_string(0).get());
        MySqlUserPtr & user = user_map[username];
        MySqlDatabasePtr database(new MySqlDatabase());
        database->set_name(userDbRes->get_string(1).get());
        user->get_databases()->push_back(database);
    }
    userDbRes->close();

    return users;
}

bool MySqlAdmin::is_root_enabled() {
    MySqlResultSetPtr res = con->query(
        "SELECT User FROM mysql.user where User = 'root' "
        "and host != 'localhost'");
    while(res->next());
    size_t row_count = res->get_row_count();
    res->close();
    return row_count != 0;
}

void MySqlAdmin::set_password(const char * username, const char * password) {
    MySqlPreparedStatementPtr stmt = con->prepare_statement(
       "UPDATE mysql.user SET Password=PASSWORD(?) WHERE User=?");
    stmt->set_string(0, password);
    stmt->set_string(1, username);
    stmt->execute();
    con->flush_privileges();
}


} } } // end namespace
