#define BOOST_TEST_IGNORE_SIGCHLD
#define BOOST_TEST_MODULE apt_tests
#include <boost/test/unit_test.hpp>


#include "nova/guest/apt.h"

using boost::optional;
using namespace nova;
using namespace nova::guest;
using std::string;
using std::stringstream;


/**---------------------------------------------------------------------------
 *- version Tests
 *---------------------------------------------------------------------------*/


BOOST_AUTO_TEST_CASE(not_real_package_returns_nothing)
{
    optional<string> version = apt::version("ghsdfhbsd");
    BOOST_REQUIRE_EQUAL(!version, true);
}

BOOST_AUTO_TEST_CASE(real_uninstalled_package_returns_nothing)
{
    optional<string> version = apt::version("cowsay");
    BOOST_REQUIRE_EQUAL(!version, true);
}

BOOST_AUTO_TEST_CASE(installed_package_returns_something)
{
    optional<string> version = apt::version("rabbitmq-server");
    BOOST_REQUIRE_EQUAL(!version, false);
    BOOST_REQUIRE_EQUAL(version.get(), "2.6.1-1");
}

BOOST_AUTO_TEST_CASE(empty_package_name_dont_crash_no_thing)
{
    optional<string> version = apt::version("");
    BOOST_REQUIRE_EQUAL(!version, true);
}
