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
