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
}
