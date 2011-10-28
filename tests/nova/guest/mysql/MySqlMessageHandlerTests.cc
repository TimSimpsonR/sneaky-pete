#define BOOST_TEST_MODULE MySqlGuestHandler_Tests
#include <boost/test/unit_test.hpp>

#include "nova/guest/mysql.h"
#include "nova/ConfigFile.h"


using nova::ConfigFile;
using nova::JsonObject;
using nova::JsonObjectPtr;
using namespace nova::guest;
using namespace std;


namespace nova { namespace guest { namespace mysql {


} } }  // end nova::guest


/*
MessageHandlerPtr create_sql() {
    MySqlGuestPtr guest(new MySqlGuest());
    MessageHandlerPtr rtn(new MySqlMessageHandler(guest));
    return rtn;
}*/


BOOST_AUTO_TEST_CASE(create_database)
{/*
    MessageHandlerPtr sql = create_sql();
    JsonObjectPtr json(new JsonObject(""));
    sql->handle_message()
    for(int i = 0; i < 1000; i ++) {
        JsonObjectPtr input(new JsonObject("{'method':'is_root_enabled'}"));
        JsonObjectPtr output = sql->handle_message(input);
        BOOST_CHECK_EQUAL(output->to_string(), "{ }");
    }*/
}
