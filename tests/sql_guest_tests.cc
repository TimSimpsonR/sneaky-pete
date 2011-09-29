#define BOOST_TEST_MODULE My_SQL_Tests
#include <boost/test/unit_test.hpp>

#include "nova/guest/sql_guest.h"
#include "nova/configfile.h"


using nova::Configfile;
using nova::JsonObject;
using nova::JsonObjectPtr;
using namespace nova::guest;
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

MessageHandlerPtr create_sql() {
    Configfile configfile("config/test-configfile.txt");
    std::string mysql_uri = configfile.get_string("mysql_uri");
    MySqlGuestPtr guest(new MySqlGuest(mysql_uri));
    MessageHandlerPtr rtn(new MySqlMessageHandler(guest));
    return rtn;
}


BOOST_AUTO_TEST_CASE(create_database)
{
    MessageHandlerPtr sql = create_sql();
    for(int i = 0; i < 1000; i ++) {
        JsonObjectPtr input(new JsonObject("{'method':'is_root_enabled'}"));
        JsonObjectPtr output = sql->handle_message(input);
        BOOST_CHECK_EQUAL(output->to_string(), "{ }");
    }
}
