#define BOOST_TEST_MODULE My_SQL_Tests
#include <boost/test/unit_test.hpp>

#include "sql_guest.h"
#include "configfile.h"


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
    const string config_location = "config/test-configfile.txt";
    Configfile configfile(config_location);
    std::string mysql_uri = configfile.get_string("mysql_uri");
    MySqlGuestPtr guest(new MySqlGuest(mysql_uri));
    MessageHandlerPtr rtn(new MySqlMessageHandler(guest));
    return rtn;
}


BOOST_AUTO_TEST_CASE(create_database)
{
    MessageHandlerPtr sql = create_sql();
    for(int i = 0; i < 1000; i ++) {
        json_object * input = json_tokener_parse(
            "{'method':'is_root_enabled'}");
        json_object * output = sql->handle_message(input);
        const char * str_output = json_object_to_json_string(output);
        BOOST_CHECK_EQUAL(str_output, "{ }");
        json_object_put(output);
        json_object_put(input);
    }
}
