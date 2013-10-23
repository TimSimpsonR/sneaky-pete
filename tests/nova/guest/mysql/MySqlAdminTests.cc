#define BOOST_TEST_MODULE My_SQL_Tests
#include <boost/test/unit_test.hpp>

#include "nova/guest/mysql/MySqlAdmin.h"
#include "nova/ConfigFile.h"
#include "nova/guest/mysql/MySqlStatements.h"

using namespace nova::guest::mysql;
using namespace std;


BOOST_AUTO_TEST_CASE(password_must_be_unique)
{
    string password;
    string last_password ;
    for(int i = 0; i < 10; i ++) {
        string password = nova::guest::mysql::generate_password();
        BOOST_CHECK_NE(last_password, password);
        last_password = password;
    }
}

BOOST_AUTO_TEST_CASE(password_must_be_kind_of_long)
{
    string password = nova::guest::mysql::generate_password();
    BOOST_CHECK_GT(password.length(), 10);
}

BOOST_AUTO_TEST_CASE(extract_user_test)
{
    string user = nova::guest::mysql::extract_user("'test'@'%'");
    BOOST_CHECK_EQUAL(user, "test");
}

BOOST_AUTO_TEST_CASE(extract_host_test)
{
    string user = nova::guest::mysql::extract_host("'test'@'%'");
    BOOST_CHECK_EQUAL(user, "%");
}

struct StringEscaper {
    const std::string escape_string(const char * const original) {
        return "I must escape this string!";
    }
};

BOOST_AUTO_TEST_CASE(test_create_globals_stmt)
{
    MySqlServerAssignments assignments;
    assignments["Empty"] = boost::blank();
    assignments["BoolTrue"] = true;
    assignments["BoolFalse"] = false;
    assignments["int"] = 42;
    assignments["double"] = 50.6;
    assignments["string"] = std::string("stuff");
    assignments["badString"] = "stuff";  // WATCH OUT!
    StringEscaper escapist;
    const auto query = create_globals_stmt<StringEscaper>(escapist,
                                                          assignments);
    BOOST_CHECK_EQUAL(
        "SET GLOBAL BoolFalse = 0;"
        "SET GLOBAL BoolTrue = 1;"
        "SET GLOBAL Empty;"
        "SET GLOBAL badString = 1;"
        "SET GLOBAL double = 50.6;"
        "SET GLOBAL int = 42;"
        "SET GLOBAL string = 'I must escape this string!';",
        query.c_str());
}
