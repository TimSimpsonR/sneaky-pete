#define BOOST_TEST_IGNORE_SIGCHLD
#define BOOST_TEST_MODULE apt_tests
#include <boost/test/unit_test.hpp>


#include "nova/guest/apt.h"
#include "nova/Log.h"
#include "nova/LogFlags.h"
#include "nova/utils/regex.h"
#include <fstream>
#include <unistd.h>

using std::ifstream;
using boost::optional;
using namespace nova;
using namespace nova::flags;
using namespace nova::guest;
using namespace nova::guest::apt;
using nova::utils::Regex;
using nova::utils::RegexMatchesPtr;
using std::string;


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

FlagMapPtr get_flags() {
    FlagMapPtr ptr(new FlagMap());
    char * test_args = getenv("TEST_ARGS");
    BOOST_REQUIRE_MESSAGE(test_args != 0,
                          "TEST_ARGS environment var not defined.");
    if (test_args != 0) {
        ptr->add_from_arg(test_args);
    }
    return ptr;
}



bool zivot_exists() {
    apt::AptGuest guest(USE_SUDO, "sneaky-pete", 1 * 60);
    return guest.version("zivot") != boost::none;
}


/**---------------------------------------------------------------------------
 *- version Tests
 *---------------------------------------------------------------------------*/

BOOST_AUTO_TEST_CASE(remove_real_package_should_succeed) {
    LogApiScope log(log_options_from_flags(get_flags()));
    if (zivot_exists()){
        apt::AptGuest guest(USE_SUDO, "sneaky-pete", 1 * 60);
        guest.remove("zivot", 60);
    }
    BOOST_REQUIRE_EQUAL(zivot_exists(), false);
}

BOOST_AUTO_TEST_CASE(not_real_package_returns_nothing)
{
    LogApiScope log(log_options_from_flags(get_flags()));
    apt::AptGuest guest(USE_SUDO, "sneaky-pete", 1 * 60);
    optional<string> version = guest.version("ghsdfhbsd", 30);
    BOOST_REQUIRE_EQUAL(version, boost::none);
}

BOOST_AUTO_TEST_CASE(installed_package_returns_something)
{
    LogApiScope log(log_options_from_flags(get_flags()));
    apt::AptGuest guest(USE_SUDO, "sneaky-pete", 1 * 60);
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
    LogApiScope log(log_options_from_flags(get_flags()));
    apt::AptGuest guest(USE_SUDO, "sneaky-pete", 1 * 60);
    // Wonder if this is could be a bug..
    // CHECK_APT_EXCEPTION(guest.version(""), UNEXPECTED_PROCESS_OUTPUT);
    CHECK_APT_EXCEPTION(guest.version(""), GENERAL);
}


/**---------------------------------------------------------------------------
 *- Install / Remove Tests
 *---------------------------------------------------------------------------*/

#define INVALID_PACKAGE_NAME "fake_package"

BOOST_AUTO_TEST_CASE(install_should_throw_exceptions_with_invalid_packages) {
    LogApiScope log(log_options_from_flags(get_flags()));
    apt::AptGuest guest(USE_SUDO, "sneaky-pete", 1 * 60);
    CHECK_APT_EXCEPTION(guest.install(INVALID_PACKAGE_NAME, 60),
                        PACKAGE_NOT_FOUND);
}


BOOST_AUTO_TEST_CASE(remove_should_throw_exceptions_with_invalid_packages) {
    LogApiScope log(log_options_from_flags(get_flags()));
    apt::AptGuest guest(USE_SUDO, "sneaky-pete", 1 * 60);
    CHECK_APT_EXCEPTION(guest.remove(INVALID_PACKAGE_NAME, 60),
                        PACKAGE_NOT_FOUND);
}

/** This is taken from the original Python tests.
 *  Because it picks on zivot it probably should not be a unit test, but for
 *  now its the best way to prove this code is working. */
BOOST_AUTO_TEST_CASE(abuse_zivot) {

    LogApiScope log(log_options_from_flags(get_flags()));
    apt::AptGuest guest(USE_SUDO, "sneaky-pete", 1 * 60);

    NOVA_LOG_DEBUG("Checking for existance...");
    const bool zivot_originally_installed = zivot_exists();
    if (zivot_originally_installed) {
        NOVA_LOG_DEBUG("Removing as part of setup...");
        guest.remove("zivot", TIME_OUT);
    }

    // At first it should not be installed.
    NOVA_LOG_DEBUG("Checking for existance #2...");
    BOOST_REQUIRE_EQUAL(zivot_exists(), false);

    NOVA_LOG_DEBUG("Checking version...");
    optional<string> version_fail = guest.version("zivot");
    BOOST_REQUIRE_EQUAL(version_fail, boost::none);

    // Can handle a second remove.
    NOVA_LOG_DEBUG("Removing...");
    guest.remove("zivot", TIME_OUT);
    BOOST_REQUIRE_EQUAL(zivot_exists(), false);

    // Should fail if time out is low.
    NOVA_LOG_DEBUG("Installing...");
    try {
        guest.install("zivot", 0.1);
        // I guess this is theoretically possible... not sure how else to test
        // this.
    } catch(const AptException & ae) {
        BOOST_REQUIRE_EQUAL(ae.code, AptException::PROCESS_TIME_OUT);
        // Wait a bit because the admin file is going to be locked while the
        // above process exits.
        sleep(3);
    }

    NOVA_LOG_DEBUG("Installing #2...");
    // Install for real.
    // Warning: If this test fails, it can be because the process created above
    // has not had time to exit, so by all means up the sleep above. But so far
    // one second is fast enough.
    guest.install("zivot", TIME_OUT);
    BOOST_REQUIRE_EQUAL(zivot_exists(), true);

    NOVA_LOG_DEBUG("Checking version once again...");
    apt::AptGuest mon_guest(USE_SUDO, "monitoring-agent", 1 * 60);
    optional<string> version_dpkg = mon_guest.version("dpkg");
    BOOST_REQUIRE_EQUAL(!!version_dpkg, true);
    Regex dpkg_regex("(\\w+)");
    RegexMatchesPtr dpkg_matches = dpkg_regex.match(version_dpkg.get().c_str(), 5);
    BOOST_REQUIRE(!!dpkg_matches);
    BOOST_REQUIRE(dpkg_matches->exists_at(0));

    NOVA_LOG_DEBUG("Checking version, even yet again more.");
    optional<string> version = guest.version("zivot");
    BOOST_REQUIRE_EQUAL(!!version, true);
    Regex regex("(\\w+)");
    RegexMatchesPtr matches = regex.match(version.get().c_str(), 5);
    BOOST_REQUIRE(!!matches);
    BOOST_REQUIRE(matches->exists_at(0));

    NOVA_LOG_DEBUG("Installing, once more...");
    // Can handle a second install.
    guest.install("zivot", TIME_OUT);
    BOOST_REQUIRE_EQUAL(zivot_exists(), true);

}


