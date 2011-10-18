#define BOOST_TEST_MODULE My_SQL_Tests
#include <boost/test/unit_test.hpp>

#include "nova/guest/mysql.h"
#include "nova/ConfigFile.h"


using nova::ConfigFile;
using nova::JsonObject;
using nova::JsonObjectPtr;
using namespace nova::guest;
using namespace nova::guest::mysql;
using sql::PreparedStatement;
using sql::ResultSet;
using sql::SQLException;
using namespace std;

/** TODO(tim.simpson): Write tests for the following functions:
    create_database,
    create_user,
    delete_database,
    disable_root,
    delete_user,
    enable_root,
    is_root_enabled,
    list_databases,
    list_users
*/

BOOST_AUTO_TEST_CASE(password_must_be_unique)
{
    string password;
    string last_password ;
    for(int i = 0; i < 10; i ++) {
        string password = MySqlGuest::generate_password();
        BOOST_CHECK_NE(last_password, password);
        last_password = password;
    }
}

BOOST_AUTO_TEST_CASE(password_must_be_kind_of_long)
{
    string password = MySqlGuest::generate_password();
    BOOST_CHECK_GT(password.length(), 10);
}

int number_of_admins(MySqlGuestPtr guest) {
    /*MySqlGuest::PreparedStatementPtr stmt = guest->prepare_statement(
        "SELECT * FROM mysql.user WHERE User='os_admin' AND Host='localhost'");
    stmt->executeUpdate();
    stmt->close();
    int number = 0;
    while(res->next()) {
        number ++;
    }
    return number;*/
}

BOOST_AUTO_TEST_CASE(add_admin)
{
    /*MySqlGuestPtr guest(new )
    int original_admin_count = number_of_admins();
    string admin_password = MySqlGuest::generate_password();
    MySqlGuest::_create_admin_user();
    int ending_admin_count = number_of_admins();
    BOOST_CHECK_GT(original_admin_count, ending_admin_count);*/
}

/*
MessageHandlerPtr create_sql() {
    ConfigFile configfile("config/test-configfile.txt");
    std::string mysql_uri = configfile.get_string("mysql_uri");
    MySqlGuestPtr guest(new MySqlGuest(mysql_uri));
    MessageHandlerPtr rtn(new MySqlMessageHandler(guest));
    return rtn;
}
*/

//TODO(tim.simpson) Copy test AddAdminUser.test_add_admin
//  This calls the add admin code directly.
//TODO(tim.simpson)
// GenerateRootPassword.test_go
// test_remove_anon_user
//   removes the anonymous user, attempts to log in
// test_remove_remote_root_access
//  removes root, attempts to log in
// InitMyCnf.test_init_mycnf
//  tests we can log in
// RestartMySql.test_restart
//


BOOST_AUTO_TEST_CASE(create_database)
{
    /*
    MessageHandlerPtr sql = create_sql();
    for(int i = 0; i < 1000; i ++) {
        JsonObjectPtr input(new JsonObject("{'method':'is_root_enabled'}"));
        JsonObjectPtr output = sql->handle_message(input);
        BOOST_CHECK_EQUAL(output->to_string(), "{ }");
    }*/
}
