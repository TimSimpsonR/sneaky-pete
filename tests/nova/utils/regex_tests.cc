#define BOOST_TEST_MODULE regex_tests
#include <boost/test/unit_test.hpp>


#include <iostream>
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

BOOST_AUTO_TEST_CASE(sql_pid_file)
{
    Regex regex("--pid-file=([a-z0-9A-Z/]+\\.pid)"); //([\\w/]+\\.pid)");
    RegexMatchesPtr matches = regex.match("user=mysql "
        "--pid-file=/var/run/mysqld/mysqld.pid "
        "--socket=/var/run/mysqld/mysqld.sock");
    BOOST_REQUIRE_EQUAL(!!matches, true);
    std::cout << "0=" << matches->get(0) << std::endl;
    std::cout << "1=" << matches->get(1) << std::endl;

}

BOOST_AUTO_TEST_CASE(match_user_db_assoc)
{
    Regex regex("^'(.+)'@");
    RegexMatchesPtr matches = regex.match("'test'@%'");
    BOOST_REQUIRE_EQUAL(!!matches, true);
    BOOST_REQUIRE_EQUAL(matches->get(1), "test");
}

BOOST_AUTO_TEST_CASE(match_user_db_assoc1)
{
    Regex regex("^'(.+)'@");
    RegexMatchesPtr matches = regex.match("'te@st@'@%'");
    BOOST_REQUIRE_EQUAL(!!matches, true);
    BOOST_REQUIRE_EQUAL(matches->get(1), "te@st@");
}

BOOST_AUTO_TEST_CASE(match_dpkg_output)
{
    Regex regex("(dpkg)\\s+(\\S*).*");
    RegexMatchesPtr matches = regex.match("dpkg   1.15.8.1+debian2");
    std::cout << "0=" << matches->get(0) << std::endl;
    std::cout << "1=" << matches->get(1) << std::endl;
    std::cout << "2=" << matches->get(2) << std::endl;
    BOOST_REQUIRE_EQUAL(!!matches, true);
    BOOST_REQUIRE_EQUAL(matches->get(1), "dpkg");
    BOOST_REQUIRE_EQUAL(matches->get(2), "1.15.8.1+debian2");
}

BOOST_AUTO_TEST_CASE(match_dpkg_output_no_version)
{
    Regex regex("(dpkg)\\s+(\\S*).*");
    RegexMatchesPtr matches = regex.match("dpkg   ");
    std::cout << "0=" << matches->get(0) << std::endl;
    std::cout << "1=" << matches->get(1) << std::endl;
    std::cout << "2=" << matches->get(2) << std::endl;
    BOOST_REQUIRE_EQUAL(!!matches, true);
    BOOST_REQUIRE_EQUAL(matches->get(1), "dpkg");
    BOOST_REQUIRE_EQUAL(matches->get(2), "");
}

BOOST_AUTO_TEST_CASE(match_dpkg_output_another_version)
{
    Regex regex("(dpkg)\\s+(\\S*).*");
    RegexMatchesPtr matches = regex.match("dpkg 1.0~rc1");
    std::cout << "0=" << matches->get(0) << std::endl;
    std::cout << "1=" << matches->get(1) << std::endl;
    std::cout << "2=" << matches->get(2) << std::endl;
    BOOST_REQUIRE_EQUAL(!!matches, true);
    BOOST_REQUIRE_EQUAL(matches->get(1), "dpkg");
    BOOST_REQUIRE_EQUAL(matches->get(2), "1.0~rc1");
}

BOOST_AUTO_TEST_CASE(match_backup_files) {
    Regex regex("^ib|^xtrabackup|^mysql$|lost|^backup-my.cnf$");
    BOOST_REQUIRE_EQUAL( true, regex.has_match("ib_logfile0"));
    BOOST_REQUIRE_EQUAL(false, regex.has_match("ie"));
    BOOST_REQUIRE_EQUAL( true, regex.has_match("xtrabackup88"));
    BOOST_REQUIRE_EQUAL(false, regex.has_match("xtrabackDown"));
    BOOST_REQUIRE_EQUAL( true, regex.has_match("mysql"));
    BOOST_REQUIRE_EQUAL(false, regex.has_match("mysql5"));
    BOOST_REQUIRE_EQUAL( true, regex.has_match("lost_and_found"));
    BOOST_REQUIRE_EQUAL(false, regex.has_match("lots"));
    BOOST_REQUIRE_EQUAL( true, regex.has_match("backup-my.cnf"));
    BOOST_REQUIRE_EQUAL(false, regex.has_match("backup-my.cnf2"));
}

BOOST_AUTO_TEST_CASE(match_empty) {
    Regex regex("^$");
    BOOST_REQUIRE_EQUAL(false, regex.has_match("Blah!"));
}

BOOST_AUTO_TEST_CASE(match_package_with_period_squeeze)
{
    #define PACKAGE_NAME_REGEX "\\S+"
    Regex regex("No packages found matching (" PACKAGE_NAME_REGEX ")\\.");
    RegexMatchesPtr matches = regex.match(
        "No packages found matching mysql-server-5.5.");
    std::cout << "0=" << matches->get(0) << std::endl;
    std::cout << "1=" << matches->get(1) << std::endl;
    BOOST_REQUIRE_EQUAL(!!matches, true);
    BOOST_REQUIRE_EQUAL(matches->get(1), "mysql-server-5.5");

    RegexMatchesPtr matches2 = regex.match(
        "dpkg-query: no packages found matching mysql-server-5.5");
    BOOST_REQUIRE_EQUAL(!matches2, true);
}

BOOST_AUTO_TEST_CASE(match_package_with_period_wheezy)
{
    #define PACKAGE_NAME_REGEX "\\S+"
    Regex regex("no packages found matching ("
        PACKAGE_NAME_REGEX ")");
    RegexMatchesPtr matches = regex.match(
        "no packages found matching mysql-server-5.5");
    std::cout << "0=" << matches->get(0) << std::endl;
    std::cout << "1=" << matches->get(1) << std::endl;
    BOOST_REQUIRE_EQUAL(matches.get() != 0, true);
    BOOST_REQUIRE_EQUAL(matches->get(1), "mysql-server-5.5");
}
