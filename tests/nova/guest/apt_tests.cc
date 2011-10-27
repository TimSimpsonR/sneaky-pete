#define BOOST_TEST_IGNORE_SIGCHLD
#define BOOST_TEST_MODULE apt_tests
#include <boost/test/unit_test.hpp>


#include "nova/guest/apt.h"
#include <fstream>
#include <unistd.h>

using std::ifstream;
using boost::optional;
using namespace nova;
using namespace nova::guest;
using namespace nova::guest::apt;
using std::string;
using std::stringstream;


const double TIME_OUT = 60;

// Asserts the given bit of code throws a JsonException instance with the given
// code.
#define CHECK_APT_EXCEPTION(statement, ex_code) try { \
        statement ; \
        BOOST_FAIL("Should have thrown."); \
    } catch(const AptException & ae) { \
        BOOST_REQUIRE_EQUAL(ae.code, AptException::ex_code); \
    }

/**---------------------------------------------------------------------------
 *- version Tests
 *---------------------------------------------------------------------------*/

BOOST_AUTO_TEST_CASE(not_real_package_returns_nothing)
{
    optional<string> version = apt::version("ghsdfhbsd", 30);
    BOOST_REQUIRE_EQUAL(version, boost::none);
}

BOOST_AUTO_TEST_CASE(real_uninstalled_package_returns_nothing)
{
    optional<string> version = apt::version("cowsay");
    BOOST_REQUIRE_EQUAL(version, boost::none);
}

BOOST_AUTO_TEST_CASE(installed_package_returns_something)
{
    optional<string> version = apt::version("rabbitmq-server");
    BOOST_REQUIRE_EQUAL(!!version, true);
    BOOST_REQUIRE(version.get() == "2.6.1-1"
                  || version.get() == "2.2.0-1~maverick0");
}

BOOST_AUTO_TEST_CASE(empty_package_name_dont_crash_no_thing)
{
    CHECK_APT_EXCEPTION(apt::version(""), UNEXPECTED_PROCESS_OUTPUT);
}

/**---------------------------------------------------------------------------
 *- Install / Remove Tests
 *---------------------------------------------------------------------------*/

#define INVALID_PACKAGE_NAME "fake_package"

BOOST_AUTO_TEST_CASE(install_should_throw_exceptions_with_invalid_packages) {
    CHECK_APT_EXCEPTION(apt::install(INVALID_PACKAGE_NAME, 60),
                        PACKAGE_NOT_FOUND);
}


BOOST_AUTO_TEST_CASE(remove_should_throw_exceptions_with_invalid_packages) {
    CHECK_APT_EXCEPTION(apt::remove(INVALID_PACKAGE_NAME, 60),
                        PACKAGE_NOT_FOUND);
}


bool cowsay_exists() {
    ifstream file("/usr/games/cowsay");
    //BOOST_REQUIRE_EQUAL(apt::version("cowsay") != boost::none, file.good());
    return file.good();
}

/** This is taken from the original Python tests.
 *  Because it picks on cowsay it probably should not be a unit test,but for
 *  now its the best way to prove this code is working. */
BOOST_AUTO_TEST_CASE(abuse_cowsay) {

    const bool cowsay_originally_installed = cowsay_exists();
    if (cowsay_originally_installed) {
        apt::remove("cowsay", TIME_OUT);
    }

    // At first it should not be installed.
    BOOST_REQUIRE_EQUAL(cowsay_exists(), false);

    // Can handle a second remove.
    apt::remove("cowsay", TIME_OUT);
    BOOST_REQUIRE_EQUAL(cowsay_exists(), false);

    // Should fail if time out is low.
    try {
        apt::install("cowsay", 0.1);
        // I guess this is theoretically possible... not sure how else to test
        // this.
    } catch(const AptException & ae) {
        BOOST_REQUIRE_EQUAL(ae.code, AptException::PROCESS_TIME_OUT);
        // Wait a bit because the admin file is going to be locked while the
        // above process exits.
        sleep(1);
    }

    // Install for real.
    // Warning: If this test fails, it can be because the process created above
    // has not had time to exit, so by all means up the sleep above. But so far
    // one second is fast enough.
    apt::install("cowsay", TIME_OUT);
    BOOST_REQUIRE_EQUAL(cowsay_exists(), true);

    // Can handle a second install.
    apt::install("cowsay", TIME_OUT);
    BOOST_REQUIRE_EQUAL(cowsay_exists(), true);

    // Remove
    apt::remove("cowsay", TIME_OUT);
    BOOST_REQUIRE_EQUAL(cowsay_exists(), false);
}


