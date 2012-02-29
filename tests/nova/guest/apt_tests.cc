#define BOOST_TEST_IGNORE_SIGCHLD
#define BOOST_TEST_MODULE apt_tests
#include <boost/test/unit_test.hpp>


#include "nova/guest/apt.h"
#include "nova/Log.h"
#include "nova/utils/regex.h"
#include <fstream>
#include <unistd.h>

using std::ifstream;
using boost::optional;
using namespace nova;
using namespace nova::guest;
using namespace nova::guest::apt;
using nova::utils::Regex;
using nova::utils::RegexMatchesPtr;
using std::string;
using std::stringstream;


const double TIME_OUT = 60;
const bool USE_SUDO = true;

// Asserts the given bit of code throws a JsonException instance with the given
// code.
#define CHECK_APT_EXCEPTION(statement, ex_code) try { \
        statement ; \
        BOOST_FAIL("Should have thrown."); \
    } catch(const AptException & ae) { \
        BOOST_REQUIRE_EQUAL(ae.code, AptException::ex_code); \
    }


struct GlobalFixture {

    LogApiScope log;

    GlobalFixture()
    : log(LogOptions::simple()) {
    }

};

BOOST_GLOBAL_FIXTURE(GlobalFixture);

/**---------------------------------------------------------------------------
 *- version Tests
 *---------------------------------------------------------------------------*/

BOOST_AUTO_TEST_CASE(not_real_package_returns_nothing)
{
    apt::AptGuest guest(USE_SUDO, "nova-guest", 1 * 60);
    optional<string> version = guest.version("ghsdfhbsd", 30);
    BOOST_REQUIRE_EQUAL(version, boost::none);
}

BOOST_AUTO_TEST_CASE(real_uninstalled_package_returns_nothing)
{
    apt::AptGuest guest(USE_SUDO, "nova-guest", 1 * 60);
    optional<string> version = guest.version("cowsay");
    BOOST_REQUIRE_EQUAL(version, boost::none);
}

BOOST_AUTO_TEST_CASE(installed_package_returns_something)
{
    apt::AptGuest guest(USE_SUDO, "nova-guest", 1 * 60);
    optional<string> version = guest.version("dpkg");
    BOOST_REQUIRE_EQUAL(!!version, true);
    Regex regex("(\\w+)\\.(\\w+)\\.(\\w+)\\.(\\w+)");
    RegexMatchesPtr matches = regex.match(version.get().c_str(), 5);
    BOOST_REQUIRE(!!matches);
    BOOST_REQUIRE(matches->exists_at(0));
    BOOST_REQUIRE(matches->exists_at(4));
}

BOOST_AUTO_TEST_CASE(empty_package_name_dont_crash_no_thing)
{
    apt::AptGuest guest(USE_SUDO, "nova-guest", 1 * 60);
    CHECK_APT_EXCEPTION(guest.version(""), UNEXPECTED_PROCESS_OUTPUT);
}

/**---------------------------------------------------------------------------
 *- Install / Remove Tests
 *---------------------------------------------------------------------------*/

#define INVALID_PACKAGE_NAME "fake_package"

BOOST_AUTO_TEST_CASE(install_should_throw_exceptions_with_invalid_packages) {
    apt::AptGuest guest(USE_SUDO, "nova-guest", 1 * 60);
    CHECK_APT_EXCEPTION(guest.install(INVALID_PACKAGE_NAME, 60),
                        PACKAGE_NOT_FOUND);
}


BOOST_AUTO_TEST_CASE(remove_should_throw_exceptions_with_invalid_packages) {
    apt::AptGuest guest(USE_SUDO, "nova-guest", 1 * 60);
    CHECK_APT_EXCEPTION(guest.remove(INVALID_PACKAGE_NAME, 60),
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

    apt::AptGuest guest(USE_SUDO, "nova-guest", 1 * 60);

    const bool cowsay_originally_installed = cowsay_exists();
    if (cowsay_originally_installed) {
        guest.remove("cowsay", TIME_OUT);
    }

    // At first it should not be installed.
    BOOST_REQUIRE_EQUAL(cowsay_exists(), false);

    // Can handle a second remove.
    guest.remove("cowsay", TIME_OUT);
    BOOST_REQUIRE_EQUAL(cowsay_exists(), false);

    // Should fail if time out is low.
    try {
        guest.install("cowsay", 0.1);
        // I guess this is theoretically possible... not sure how else to test
        // this.
    } catch(const AptException & ae) {
        BOOST_REQUIRE_EQUAL(ae.code, AptException::PROCESS_TIME_OUT);
        // Wait a bit because the admin file is going to be locked while the
        // above process exits.
        sleep(3);
    }

    // Install for real.
    // Warning: If this test fails, it can be because the process created above
    // has not had time to exit, so by all means up the sleep above. But so far
    // one second is fast enough.
    guest.install("cowsay", TIME_OUT);
    BOOST_REQUIRE_EQUAL(cowsay_exists(), true);

    // Can handle a second install.
    guest.install("cowsay", TIME_OUT);
    BOOST_REQUIRE_EQUAL(cowsay_exists(), true);

    // Remove
    guest.remove("cowsay", TIME_OUT);
    BOOST_REQUIRE_EQUAL(cowsay_exists(), false);
}


