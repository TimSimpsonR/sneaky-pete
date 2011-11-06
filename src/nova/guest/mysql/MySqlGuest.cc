#include "nova/guest/guest.h"
#include "nova/Log.h"
#include <boost/foreach.hpp>
#include <boost/format.hpp>
#include <fstream>
#include <iostream>
#include <memory>
//#include <mysql/mysql.h>
#include "nova/guest/mysql.h"
#include <sstream>

using boost::format;
using boost::none;
using boost::optional;
using nova::Log;
using namespace std;


namespace nova { namespace guest { namespace mysql {

namespace {

    Log log;

}
/**---------------------------------------------------------------------------
 *- MySqlGuest
 *---------------------------------------------------------------------------*/

MySqlGuest::MySqlGuest(MySqlConnectionPtr con)
: con(con)
{
}

MySqlGuest::~MySqlGuest() {
}

void MySqlGuest::create_database(MySqlDatabaseListPtr databases) {
    BOOST_FOREACH(MySqlDatabasePtr & db, *databases) {
        string text = str(format(
            "CREATE DATABASE IF NOT EXISTS `%s` CHARACTER SET = `%s` "
            "COLLATE = `%s`") % con->escape_string(db->get_name().c_str())
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

void MySqlGuest::create_user(MySqlUserPtr user, const char * host) {
    if (!user->get_password()) {
        throw MySqlException(MySqlException::NO_PASSWORD_FOR_CREATE_USER);
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

void MySqlGuest::create_users(MySqlUserListPtr users) {
    BOOST_FOREACH(MySqlUserPtr & user, *users) {
        create_user(user);
    }
}

void MySqlGuest::delete_database(const string & database_name) {
    string text = str(format("DROP DATABASE IF EXISTS `%s`")
                       % con->escape_string(database_name.c_str()));
    con->query(text.c_str());
    con->flush_privileges();
}

void MySqlGuest::delete_user(const string & username) {
    string text = str(format("DROP USER `%s`")
                       % con->escape_string(username.c_str()));
    con->query(text.c_str());
    con->flush_privileges();
}

MySqlUserPtr MySqlGuest::enable_root() {
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
    con->flush_privileges();
    return root_user;
}

MySqlDatabaseListPtr MySqlGuest::list_databases() {
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

MySqlUserListPtr MySqlGuest::list_users() {
    MySqlUserListPtr users(new MySqlUserList());
    MySqlResultSetPtr res = con->query(
        "select User from mysql.user where host != 'localhost'");
    while (res->next()) {
        MySqlUserPtr user(new MySqlUser());
        user->set_name(res->get_string(0).get());
        users->push_back(user);
    }
    res->close();
    return users;
}

bool MySqlGuest::is_root_enabled() {
    MySqlResultSetPtr res = con->query(
        "SELECT User FROM mysql.user where User = 'root' "
        "and host != 'localhost'");
    while(res->next());
    size_t row_count = res->get_row_count();
    res->close();
    return row_count != 0;
}

void MySqlGuest::set_password(const char * username, const char * password) {
    //TODO: Use prepared statement.
    //PreparedStatementPtr stmt = con->prepare_statement(
    //    "UPDATE mysql.user SET Password=PASSWORD(?) WHERE User=?");
    //stmt->set_string(1, password);
    //stmt->set_string(2, username);
    //stmt->execute();
    //stmt->close();
    string text = str(format("UPDATE mysql.user SET Password=PASSWORD(\"%s\") "
                             "WHERE User=\"%s\"") % password % username);
    con->query(text.c_str());
    con->flush_privileges();
}


} } } // end namespace
