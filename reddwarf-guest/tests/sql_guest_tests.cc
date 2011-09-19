#define BOOST_TEST_MODULE My_SQL_Tests
#include <boost/test/unit_test.hpp>

#include "sql_guest.h"


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

MySqlGuestPtr create_sql() {
    MySqlGuestPtr guest(new MySqlGuest());
    return guest;
}

BOOST_AUTO_TEST_CASE(create_database)
{
    //MySqlGuestPtr guest = create_sql();
    //guest->create_database("");
}
