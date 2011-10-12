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

MySqlGuest::MySqlGuest(const std::string & uri)
:   driver(0), con(0)
{
}

MySqlGuest::~MySqlGuest() {
}

MySqlGuest::PreparedStatementPtr MySqlGuest::prepare_statement(const char * text) {
    MySqlGuest::PreparedStatementPtr stmt;
    return stmt;
}

string MySqlGuest::create_user(const string & username,const string & password,
                               const string & host) {
    return "";
}

MySQLUserListPtr MySqlGuest::list_users() {
    MySQLUserListPtr users(new MySQLUserList());
    return users;
}
string MySqlGuest::delete_user(const string & username) {
    return "";
}
string MySqlGuest::create_database(const string & database_name, const string & character_set, const string & collate) {
    return "";
}
MySQLDatabaseListPtr MySqlGuest::list_databases() {
    MySQLDatabaseListPtr databases(new MySQLDatabaseList());
    return databases;
}

string MySqlGuest::delete_database(const string & database_name) {
    return "";
}

string MySqlGuest::enable_root() {
    return "";
}

string MySqlGuest::disable_root() {
    return "";
}

bool MySqlGuest::is_root_enabled() {
    return false;
}

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
