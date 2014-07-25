#define BOOST_TEST_IGNORE_SIGCHLD
#define BOOST_TEST_MODULE CommandsTests
#include <boost/test/unit_test.hpp>

#include "nova/guest/redis/commands.h"

using namespace std;
using namespace nova::redis;


BOOST_AUTO_TEST_CASE(CommandsTests) {
    Commands cmd("12345");

    BOOST_REQUIRE_EQUAL(cmd.auth(),
                        "*2\r\n$4\r\nAUTH\r\n$5\r\n12345\r\n");
    BOOST_REQUIRE_EQUAL(cmd.ping(),
                        "*1\r\n$4\r\nPING\r\n");

    BOOST_REQUIRE_EQUAL(cmd.client_set_name("TIBERIUS"),
        "*3\r\n"
        "$6\r\nCLIENT\r\n"
        "$7\r\nSETNAME\r\n"
        "$8\r\n"
        "TIBERIUS\r\n");

    BOOST_REQUIRE_EQUAL(cmd.config_set("key", "value"),
        "*4\r\n"
        "$6\r\nCONFIG\r\n"
        "$3\r\nSET\r\n"
        "$3\r\nkey\r\n"
        "$5\r\nvalue\r\n");

    BOOST_REQUIRE_EQUAL(cmd.config_get("key"),
        "*3\r\n"
        "$6\r\nCONFIG\r\n"
        "$3\r\nGET\r\n"
        "$3\r\nkey\r\n");

    BOOST_REQUIRE_EQUAL(cmd.config_rewrite(),
        "*2\r\n"
        "$6\r\nCONFIG\r\n"
        "$7\r\nREWRITE\r\n");

    //BOOST_REQUIRE_EQUAL(cmd.config_get("HAT"),
    //    "$3\r\nHAT\r\n")
}
