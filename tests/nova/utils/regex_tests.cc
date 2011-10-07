#define BOOST_TEST_MODULE regex_tests
#include <boost/test/unit_test.hpp>


#include "nova/utils/regex.h"

using std::endl;
using namespace nova;
using namespace nova::utils;
using std::string;
using std::stringstream;


/**---------------------------------------------------------------------------
 *- Regex Tests
 *---------------------------------------------------------------------------*/

BOOST_AUTO_TEST_CASE(one_match)
{
    Regex regex("^\\w+\\s*=\\s*['\"]?(.[^'\"]*)['\"]?\\s*$");
    RegexMatchesPtr matches = regex.match("something = root");
    BOOST_REQUIRE_EQUAL(false, !matches);

    BOOST_REQUIRE_EQUAL(matches->exists_at(-1), false);
    BOOST_CHECK_THROW(matches->get(-1), std::out_of_range);

    BOOST_REQUIRE_EQUAL(true, matches->exists_at(0));
    BOOST_CHECK_EQUAL(matches->get(0), "something = root");

    BOOST_CHECK_EQUAL(true, matches->exists_at(0));
    for (int i = 2; i < 6; i ++) {
        BOOST_CHECK_EQUAL(false, matches->exists_at(i));
        BOOST_CHECK_THROW(matches->get(i), std::out_of_range);
    }
    BOOST_CHECK_EQUAL(matches->get(1), "root");
}

BOOST_AUTO_TEST_CASE(one_match_2)
{
    Regex regex("\\s*([a-z]+)\\s*");
    RegexMatchesPtr matches = regex.match("#! user =    blah  ");
    BOOST_REQUIRE_EQUAL(false, !matches);

    BOOST_REQUIRE_EQUAL(matches->exists_at(-1), false);
    BOOST_CHECK_THROW(matches->get(-1), std::out_of_range);

    BOOST_REQUIRE_EQUAL(matches->exists_at(0), true);
    BOOST_CHECK_EQUAL(matches->get(0), " user ");

    BOOST_REQUIRE_EQUAL(matches->exists_at(1), true);
    BOOST_CHECK_EQUAL(matches->get(1), "user");

    BOOST_REQUIRE_EQUAL(matches->exists_at(2), false);
    BOOST_CHECK_THROW(matches->get(2), std::out_of_range);
}

BOOST_AUTO_TEST_CASE(no_match)
{
    Regex regex("\\s*([a-z]+)\\s*");
    RegexMatchesPtr matches = regex.match("#&?@$!");
    BOOST_REQUIRE_EQUAL(true, !matches);
}

BOOST_AUTO_TEST_CASE(match_within_many_lines) {
    Regex regex("Setting up (\\S+)\\s+");
    stringstream output;
    output
    << "Authentication warning overridden." << endl
    << "Selecting previously deselected package figlet." << endl
    << "(Reading database ... 78885 files and directories currently installed.)"
        << endl
    << "Unpacking figlet (from .../figlet_2.2.2-1ubuntu1_i386.deb) ..." << endl
    << "Processing triggers for man-db ..." << endl
    << "Setting up figlet (2.2.2-1ubuntu1) ..." << endl
    << "update-alternatives: using /usr/bin/figlet-figlet to provide "
        "/usr/bin/figlet (figlet) in auto mode" << endl;
    RegexMatchesPtr matches = regex.match(output.str().c_str());
    BOOST_REQUIRE_EQUAL(true, !!matches);
    BOOST_CHECK_EQUAL(matches->get(0), "Setting up figlet ");
    BOOST_CHECK_EQUAL(matches->get(1), "figlet");
}
