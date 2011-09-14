#include "guest.h"
using namespace std;

Guest::Guest() {
    driver = get_driver_instance();
    con = driver->connect("tcp://127.0.0.1:3306", "root", "");
    // Connect to the MySQL test database
    con->setSchema("mysql");
}

Guest::~Guest() {
    delete con;
}

string Guest::create_user(const string & username,const string & password, const string & host) {
    sql::PreparedStatement *stmt;
    try {
        stmt = con->prepareStatement("insert into mysql.user (Host, User, Password) values (?, ?, password(?))");
        stmt->setString(1, host.c_str());
        stmt->setString(2, username.c_str());
        stmt->setString(3, password.c_str());
        stmt->execute();
        stmt->close();
        delete stmt;
    } catch (sql::SQLException &e) {
        delete stmt;
        throw e;
    }
    
    return "Guest::create_user method";
}
string Guest::list_users() {
    sql::PreparedStatement *stmt;
    sql::ResultSet *res;
    try {
        stmt = con->prepareStatement("select User from mysql.user where host != 'localhost';");
        res = stmt->executeQuery();
        while (res->next()) {
            syslog(LOG_INFO, "username: %s", res->getString("User").c_str());
        }
        res->close();
        stmt->close();
        delete res;
        delete stmt;
    } catch (sql::SQLException &e) {
        delete res;
        delete stmt;
        throw e;
    }
    return "Guest::list_users method";    
}
string Guest::delete_user(const string & username) {
    sql::PreparedStatement *stmt;
    try {
        stmt = con->prepareStatement("DROP USER `?`");
        stmt->setString(1, username);
        stmt->executeUpdate();
        stmt->close();
        delete stmt;
    } catch (sql::SQLException &e) {
        delete stmt;
        throw e;
    }

    return "Guest::delete_user method";
}
string Guest::create_database(const string & database_name, const string & character_set, const string & collate) {
    sql::PreparedStatement *stmt;
    try {
        stmt = con->prepareStatement("CREATE DATABASE IF NOT EXISTS `?` CHARACTER SET = ? COLLATE = ?;");
        stmt->setString(1, database_name);
        stmt->setString(2, character_set);
        stmt->setString(3, collate);
        stmt->executeUpdate();
        stmt->close();
        delete stmt;
    } catch (sql::SQLException &e) {
        delete stmt;
        throw e;
    }
        
    return "Guest::create_database method";
}
string Guest::list_databases() {
    return "Guest::list_databases method";   
}
string Guest::delete_database(const string & database_name) {
    return "Guest::delete_database method";
}
string Guest::enable_root() {
    return "Guest::enable_root method";
}
string Guest::disable_root() {
    return "Guest::disable_root method";
}
string Guest::is_root_enabled() {
    return "Guest::is_root_enabled method";
}
