#include "guest.h"
#include <memory>
#include <uuid/uuid.h>

using sql::PreparedStatement;
using sql::ResultSet;
using sql::SQLException;
using namespace std;


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

Guest::Guest() {
    try {
        driver = get_driver_instance();
        con = driver->connect("tcp://127.0.0.1:3306", "root", "");
        // Connect to the MySQL test database
        con->setSchema("mysql");
    } catch (SQLException &e) {
        syslog(LOG_ERR, "Error with connection %s", e.what());
        delete con;
        throw e;
    }

}

Guest::~Guest() {
    delete con;
}

Guest::PreparedStatementPtr Guest::prepare_statement(const char * text) {
    auto_ptr<PreparedStatement> stmt(con->prepareStatement(text));
    return stmt;
}

string Guest::create_user(const string & username,const string & password, const string & host) {
    PreparedStatementPtr stmt = prepare_statement(
        "insert into mysql.user (Host, User, Password) values (?, ?, password(?))");
    stmt->setString(1, host.c_str());
    stmt->setString(2, username.c_str());
    stmt->setString(3, password.c_str());
    stmt->executeUpdate();
    stmt->close();
    return "Guest::create_user method";
}
MySQLUserListPtr Guest::list_users() {
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
string Guest::delete_user(const string & username) {
    PreparedStatementPtr stmt = prepare_statement("DROP USER `?`");
    stmt->setString(1, username);
    stmt->executeUpdate();
    stmt->close();
    return "Guest::delete_user method";
}
string Guest::create_database(const string & database_name, const string & character_set, const string & collate) {
    PreparedStatementPtr stmt = prepare_statement(
        "CREATE DATABASE IF NOT EXISTS `?` CHARACTER SET = ? COLLATE = ?;");
    stmt->setString(1, database_name);
    stmt->setString(2, character_set);
    stmt->setString(3, collate);
    stmt->executeUpdate();
    stmt->close();
    return "Guest::create_database method";
}
string Guest::list_databases() {
    return "Guest::list_databases method";
}
string Guest::delete_database(const string & database_name) {
    return "Guest::delete_database method";
}
string Guest::enable_root() {
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

string Guest::disable_root() {
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

bool Guest::is_root_enabled() {
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
