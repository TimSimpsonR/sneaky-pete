#include "nova/guest/guest.h"
#include "nova/Log.h"
#include <boost/foreach.hpp>
#include <fstream>
#include <iostream>
#include <memory>
#include "nova/guest/mysql.h"
#include <sstream>
#include <regex.h>
#include <uuid/uuid.h>


using nova::Log;
using sql::PreparedStatement;
using sql::ResultSet;
using sql::SQLException;
using namespace std;


namespace nova { namespace guest { namespace mysql {

/**---------------------------------------------------------------------------
 *- MySqlGuest
 *---------------------------------------------------------------------------*/

namespace {

    Log log;

    void append_match_to_string(string & str, regex_t regex,
                                const char * line) {
        // 2 matches, the 2nd is the ()
        int num_matches = 2;
        regmatch_t matches[num_matches];
        if (regexec(&regex, line, num_matches, matches, 0) == 0) {
            const regoff_t & start_index = matches[num_matches-1].rm_so;
            const regoff_t & end_index = matches[num_matches-1].rm_eo;
            const char* begin_of_new_string = line + start_index;
            const regoff_t match_size = end_index - start_index;
            if (match_size > 0) {
                str.append(begin_of_new_string, match_size);
            }
        }
    }

    void get_username_and_password_from_config_file(string & user,
                                                    string & password) {
        const char *pattern = "^\\w+\\s*=\\s*['\"]?(.[^'\"]*)['\"]?\\s*$";
        regex_t regex;
        regcomp(&regex, pattern, REG_EXTENDED);

        ifstream my_cnf("/etc/mysql/my.cnf");
        if (!my_cnf.is_open()) {
            throw MySqlGuestException(MySqlGuestException::MY_CNF_FILE_NOT_FOUND);
        }
        std::string line;
        //char  tmp[256]={0x0};
        bool is_in_client = false;
        while(my_cnf.good()) {
        //while(fp!=NULL && fgets(tmp, sizeof(tmp) -1,fp) != NULL)
        //{
            getline(my_cnf, line);
            if (strstr(line.c_str(), "[client]")) {
                is_in_client = true;
            }
            if (strstr(line.c_str(), "[mysqld]")) {
                is_in_client = false;
            }
            // Be careful - end index is non-inclusive.
            if (is_in_client && strstr(line.c_str(), "user")) {
                append_match_to_string(user, regex, line.c_str());
            }
            if (is_in_client && strstr(line.c_str(), "password")) {
                append_match_to_string(password, regex, line.c_str());
            }
        }
        my_cnf.close();

        regfree(&regex);
    }
}

MySqlGuest::MySqlGuest(const std::string & uri)
:   driver(0), con(0)
{
    string user;
    string password;

    get_username_and_password_from_config_file(user, password);
    log.info2("got these back %s, %s", user.c_str(), password.c_str());
    driver = get_driver_instance();
    log.info2("Connecting to %s with username:%s, password:%s",
              uri.c_str(), user.c_str(), password.c_str());
    con = driver->connect(uri, user, password);
    try {
        // Connect to the MySQL test database
        con->setSchema("mysql");
    } catch (SQLException &e) {
        log.info2("Error with connection %s", e.what());
        delete con;
        throw e;
    }
}

MySqlGuest::~MySqlGuest() {
    delete con;
}


void MySqlGuest::create_database(MySqlDatabaseListPtr databases) {
    BOOST_FOREACH(MySqlDatabasePtr & db, *databases) {
        PreparedStatementPtr stmt = prepare_statement(
            "CREATE DATABASE IF NOT EXISTS `?` CHARACTER SET = ? COLLATE = ?;");
        stmt->setString(1, db->get_name());
        stmt->setString(2, db->get_character_set());
        stmt->setString(3, db->get_collation());
        stmt->executeUpdate();
        stmt->close();
    }
}

void MySqlGuest::create_user(MySqlUserPtr user) {
    const char * host = "%";
    PreparedStatementPtr stmt = prepare_statement(
        "GRANT USAGE ON *.* TO '?'@'?' IDENTIFIED BY '?';");
    stmt->setString(1, user->get_name().c_str());
    stmt->setString(2, host);
    stmt->setString(3, user->get_password().c_str());
    stmt->executeUpdate();
    stmt->close();
}

void MySqlGuest::create_users(MySqlUserListPtr users) {
    BOOST_FOREACH(MySqlUserPtr & user, *users) {
        create_user(user);
    }
}

void MySqlGuest::delete_database(const string & database_name) {
    throw std::exception(); // TODO
    //return "MySqlGuest::delete_database method";
}

void MySqlGuest::delete_user(const string & username) {
    PreparedStatementPtr stmt = prepare_statement("DROP USER `?`");
    stmt->setString(1, username);
    stmt->executeUpdate();
    stmt->close();
}


void MySqlGuest::disable_root() {
    {
        PreparedStatementPtr stmt = prepare_statement(
            "DELETE FROM mysql.user where User='root' and Host!='localhost'");
        stmt->executeUpdate();
        stmt->close();
    }

    string new_password = generate_password();
    log.info2("generate %s", new_password.c_str());
    this->set_password("root", new_password.c_str());
}

MySqlUserPtr MySqlGuest::enable_root() {
    MySqlUserPtr root_user(new MySqlUser());
    root_user->set_name("root");
    root_user->set_password(generate_password());
    create_user(root_user);
    set_password("root", root_user->get_password().c_str());
    grand_all_privileges("root", "%");
    flush_privileges();
    return root_user;
}

void MySqlGuest::flush_privileges() {
    PreparedStatementPtr stmt = prepare_statement(
        "FLUSH PRIVILEGES;");
    stmt->executeUpdate();
    stmt->close();
}

string MySqlGuest::generate_password() {
    uuid_t id;
    uuid_generate(id);
    char *buf = new char[37];
    uuid_unparse(id, buf);
    uuid_clear(id);
    return string(buf);
}


void MySqlGuest::grand_all_privileges(const char * username,
                                      const char * host) {
    PreparedStatementPtr stmt = prepare_statement(
        "GRANT ALL PRIVILEGES ON *.* TO '?'@'?' WITH GRANT OPTION;");
    stmt->setString(1, username);
    stmt->setString(2, host);
    stmt->executeUpdate();
    stmt->close();
}

MySqlDatabaseListPtr MySqlGuest::list_databases() {
    MySqlDatabaseListPtr databases(new MySqlDatabaseList());
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
        MySqlDatabasePtr database(new MySqlDatabase());
        database->set_name(res->getString("name"));
        database->set_collation(res->getString("collation"));
        database->set_character_set(res->getString("charset"));
        databases->push_back(database);
    }
    res->close();
    stmt->close();
    return databases;
}

MySqlUserListPtr MySqlGuest::list_users() {
    MySqlUserListPtr users(new MySqlUserList());
    PreparedStatementPtr stmt = prepare_statement(
        "select User from mysql.user where host != 'localhost';");
    auto_ptr<ResultSet> res(stmt->executeQuery());
    while (res->next()) {
        MySqlUserPtr user(new MySqlUser());
        user->set_name(res->getString("User"));
        users->push_back(user);
    }
    res->close();
    stmt->close();
    return users;
}

MySqlGuest::PreparedStatementPtr MySqlGuest::prepare_statement(const char * text) {
    auto_ptr<PreparedStatement> stmt(con->prepareStatement(text));
    return stmt;
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

void MySqlGuest::set_password(const char * username, const char * password) {
    PreparedStatementPtr stmt = prepare_statement(
        "UPDATE mysql.user SET Password=PASSWORD(?) WHERE User='?'");
    stmt->setString(1, password);
    stmt->setString(2, username);
    stmt->executeUpdate();
    stmt->close();
}


} } } // end namespace
