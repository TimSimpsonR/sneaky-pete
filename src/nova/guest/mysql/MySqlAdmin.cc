#include "pch.hpp"
#include "nova/guest/guest.h"
#include "nova/Log.h"
#include <boost/foreach.hpp>
#include <boost/format.hpp>
#include <boost/algorithm/string/join.hpp>
#include <vector>

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
using boost::make_tuple;
using boost::none;
using boost::optional;
using nova::Log;
using nova::utils::Regex;
using nova::utils::RegexMatchesPtr;
using namespace std;
using boost::tuple;


namespace nova { namespace guest { namespace mysql {

namespace {
    typedef map<string, MySqlUserPtr> UserMap;
}

string extract_user(const string & user) {
    Regex regex("^'(.+)'@");
    RegexMatchesPtr matches = regex.match(user.c_str());
    if (matches && matches->exists_at(1)) {
        return matches->get(1);
    } else {
        NOVA_LOG_ERROR2("Failed to parse the grantee: %s", user.c_str());
        throw MySqlGuestException(MySqlGuestException::GENERAL);
    }
}

string extract_host(const string & user) {
    Regex regex("^'.+'@'(.+)'");
    RegexMatchesPtr matches = regex.match(user.c_str());
    if (matches && matches->exists_at(1)) {
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

void MySqlAdmin::change_passwords(MySqlUserListPtr users){
    BOOST_FOREACH(MySqlUserPtr & user, *users) {
        set_password(user->get_name().c_str(), user->get_host().c_str(),
                     user->get_password().get().c_str());
    }
}

void MySqlAdmin::update_attributes(const string & username, const string & hostname, MySqlUserAttrPtr user){
    std::vector<std::string> subquery;
    MySqlPreparedStatementPtr stmt;
    const MySqlUserPtr found_user = find_user(username, hostname);
    MySqlDatabaseListPtr db_access = list_access(username, hostname);
    std::string host_name;
    if (user->get_name()){
        const string name = str(format(" User='%s' ") 
                          % con->escape_string(user->get_name().get().c_str()));
        subquery.push_back(name);
    }
    if (user->get_host()){
        const string host = str(format(" Host='%s' ") 
                          % con->escape_string(user->get_host().get().c_str()));
        subquery.push_back(host);
    }
    if (user->get_password()){
        const string password = str(format(" Password=PASSWORD('%s') ") 
                          % con->escape_string(user->get_password().get().c_str()));
        subquery.push_back(password);
    }
    std::string final_subquery = boost::algorithm::join(subquery, ", ");
    string final_query = str(format("UPDATE mysql.user SET %s"
                             " WHERE User = '%s' AND "
                             " Host = '%s'" )
                             % final_subquery.c_str()
                             % con->escape_string(found_user->get_name().c_str())
                             % con->escape_string(found_user->get_host().c_str()));
    con->query(final_query.c_str());
    con->flush_privileges();
    if (user->get_name()){
        if (!(user->get_host())){
            host_name = found_user->get_host();
        }
        else{
            host_name = user->get_host().get();
        }
        grant_access(user->get_name().get(), host_name, db_access);
    }
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

void MySqlAdmin::create_user(MySqlUserPtr user) {
    if (!user->get_password()) {
        throw MySqlGuestException(MySqlGuestException::NO_PASSWORD_FOR_CREATE_USER);
    }
    string text = str(format(
        "GRANT USAGE ON *.* TO '%s'@\"%s\" IDENTIFIED BY '%s'")
        % con->escape_string(user->get_name().c_str())
        % con->escape_string(user->get_host().c_str())
        % con->escape_string(user->get_password().get().c_str()));
    con->query(text.c_str());

    BOOST_FOREACH(MySqlDatabasePtr db, *user->get_databases()) {
        string text = str(format(
            "GRANT ALL PRIVILEGES ON `%s`.* TO `%s`@'%s'")
            % con->escape_string(db->get_name().c_str())
            % con->escape_string(user->get_name().c_str())
            % con->escape_string(user->get_host().c_str()));
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

void MySqlAdmin::delete_user(const string & username, const string & hostname) {
    string text = str(format("DROP USER `%s`@`%s`")
                       % con->escape_string(username.c_str())
                       % con->escape_string(hostname.c_str()));
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
    set_password("root", "%", root_user->get_password().get().c_str());
    con->grant_all_privileges("root", "%");
    con->revoke_privileges("FILE", "*", "root", "%");
    con->flush_privileges();
    return root_user;
}

MySqlUserPtr MySqlAdmin::find_user(const std::string & username, const std::string & hostname) {
    string query = str(format(" SELECT"
                         "   User, Host, Password"
                         " FROM"
                         "   mysql.user"
                         " WHERE"
                         "   Host != 'localhost'"
                         " AND"
                         "   User = '%s'"
                         " AND"
                         "  Host = '%s'"
                         " ORDER BY 1")
                         % con->escape_string(username.c_str())
                         % con->escape_string(hostname.c_str()));

    MySqlResultSetPtr res = con->query(query.c_str());
    if (!res->next()) {
        NOVA_LOG_ERROR2("Could not find a user named %s@%s", username.c_str(), hostname.c_str());
        throw MySqlGuestException(MySqlGuestException::USER_NOT_FOUND);
    }

    MySqlUserPtr user(new MySqlUser);
    user->set_name(res->get_string(0).get());
    user->set_host(res->get_string(1).get());
    user->set_password(res->get_string(2).get());

    // Get the list of users and their associated databases
    MySqlResultSetPtr userDbRes = con->query(
                "SELECT grantee, table_schema "
                "FROM information_schema.SCHEMA_PRIVILEGES "
                "WHERE privilege_type != 'USAGE' "
                "GROUP BY grantee, table_schema;");
    while(userDbRes->next()) {
        string found_username = extract_user(userDbRes->get_string(0).get());
        string found_hostname = extract_host(userDbRes->get_string(0).get());
        if ((found_username == username) && (found_hostname == hostname)) {
            MySqlDatabasePtr database(new MySqlDatabase());
            database->set_name(userDbRes->get_string(1).get());
            user->get_databases()->push_back(database);
        }
    }
    userDbRes->close();

    return user;
}

void MySqlAdmin::grant_access(const std::string & user_name, const std::string & host_name, MySqlDatabaseListPtr databases) {
    MySqlUserPtr user = find_user(user_name, host_name);
    BOOST_FOREACH(const MySqlDatabasePtr & db, *databases) {
        con->grant_privileges("ALL", db->get_name().c_str(), user->get_name().c_str(), user->get_host().c_str());
    }
    con->flush_privileges();
}

boost::tuple<MySqlDatabaseListPtr, boost::optional<string> >
MySqlAdmin::list_databases(unsigned int limit, optional<string> marker,
                           bool include_marker)
{
    if (limit == 0) {
        throw MySqlGuestException(MySqlGuestException::INVALID_ZERO_LIMIT);
    }
    stringstream query;
    query << "   SELECT"
        "            schema_name as name,"
        "            default_character_set_name as charset,"
        "            default_collation_name as collation"
        "        FROM"
        "            information_schema.schemata"
        "        WHERE"
        "            schema_name not in"
        "            ('mysql', 'information_schema', 'lost+found')";
    if (marker) {
        query << "\n AND schema_name ";
        if (include_marker) {
            query << " >= ";
        } else {
            query << " > ";
        }
        query << "'" << con->escape_string(marker.get().c_str()) << "'";
    }
    query <<
        "        ORDER BY"
        "            schema_name ASC"
        "        LIMIT " << limit + 1;

    MySqlResultSetPtr res = con->query(query.str().c_str());

    MySqlDatabaseListPtr databases(new MySqlDatabaseList());
    MySqlDatabasePtr database;
    optional<string> next_marker(boost::none);
    while (res->next()) {
        if (databases->size() >= limit) {
            // We asked for limit + 1, so we actually don't want to return
            // this value. We just make sure something past the limit exists
            // to know if we should return a marker.
            next_marker = database->get_name();
            break;
        }
        database.reset(new MySqlDatabase());
        database->set_name(res->get_string(0).get());
        database->set_character_set(res->get_string(1).get());
        database->set_collation(res->get_string(2).get());
        databases->push_back(database);
    }
    res->close();
    return boost::make_tuple(databases, next_marker);
}

boost::tuple<MySqlUserListPtr, boost::optional<string> >
MySqlAdmin::list_users(unsigned int limit, optional<string> marker,
                       bool include_marker)
{
    if (limit == 0) {
        throw MySqlGuestException(MySqlGuestException::INVALID_ZERO_LIMIT);
    }
    UserMap user_map;
    // Get the list of users
    stringstream query;
    query << "SELECT User, Host, Marker FROM"
             " (SELECT User, Host, CONCAT(User, '@', Host) as Marker"
             " FROM mysql.user) as innerquery"
             " WHERE host != 'localhost'";
    if (marker) {
        query << " AND Marker ";
        if (include_marker) {
            query << " >= ";
        } else {
            query << " > ";
        }
        query << "'" << con->escape_string(marker.get().c_str()) << "'";
    }
    query << " ORDER BY Marker ASC LIMIT " << limit + 1;

    MySqlResultSetPtr res = con->query(query.str().c_str());
    MySqlUserListPtr users(new MySqlUserList());
    MySqlUserPtr user;
    optional<string> next_marker(boost::none);
    while (res->next()) {
        if (users->size() >= limit) {
            // We asked for limit + 1, so we actually don't want to return
            // this value. We just make sure something past the limit exists
            // to know if we should return a marker.
            next_marker = user->get_name() + "@" + user->get_host();
            break;
        }
        user.reset(new MySqlUser());
        user->set_name(res->get_string(0).get());
        user->set_host(res->get_string(1).get());
        users->push_back(user);
        string mapkey = user->get_name() + "@" + user->get_host();
        user_map[mapkey] = user;
    }
    res->close();

    // Get the list of users and their associated databases
    MySqlResultSetPtr userDbRes = con->query(
                "SELECT grantee, table_schema from information_schema.SCHEMA_PRIVILEGES "
                "WHERE privilege_type != 'USAGE' "
                "GROUP BY grantee, table_schema;");
    while(userDbRes->next()) {
        string username = extract_user(userDbRes->get_string(0).get());
        string hostname = extract_host(userDbRes->get_string(0).get());
        string mapkey = username + "@" + hostname;
        if (user_map.find(mapkey) != user_map.end()) {
            MySqlUserPtr & user = user_map[mapkey];
            MySqlDatabasePtr database(new MySqlDatabase());
            database->set_name(userDbRes->get_string(1).get());
            user->get_databases()->push_back(database);
        }
    }
    userDbRes->close();

    return boost::make_tuple(users, next_marker);
}

bool MySqlAdmin::is_root_enabled() {
    MySqlResultSetPtr res = con->query(
        "SELECT User FROM mysql.user WHERE User = 'root' "
        "AND Host != 'localhost'");
    while(res->next());
    size_t row_count = res->get_row_count();
    res->close();
    return row_count != 0;
}

MySqlDatabaseListPtr MySqlAdmin::list_access(const std::string & user_name, const std::string & host_name){
    MySqlUserPtr user = find_user(user_name, host_name);
    return user->get_databases();
}

void MySqlAdmin::revoke_access(const std::string & user_name, const std::string & host_name, const std::string & database_name){
    MySqlUserPtr user = find_user(user_name, host_name);
    con->revoke_privileges("ALL", database_name.c_str(), user_name.c_str(), host_name.c_str());
    con->flush_privileges();
}

void MySqlAdmin::set_password(const char * username, const char * hostname, const char * password) {
    MySqlPreparedStatementPtr stmt = con->prepare_statement(
       "UPDATE mysql.user SET Password=PASSWORD(?) WHERE User=? AND Host=?");
    stmt->set_string(0, password);
    stmt->set_string(1, username);
    stmt->set_string(2, hostname);
    stmt->execute();
    con->flush_privileges();
}


} } } // end namespace
