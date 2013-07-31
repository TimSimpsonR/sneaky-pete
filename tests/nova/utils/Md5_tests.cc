#define BOOST_TEST_MODULE json_tests
#include <boost/test/unit_test.hpp>
#include <boost/test/unit_test_monitor.hpp>

#include "nova/utils/Md5.h"

using namespace nova::utils;
using std::string;


#define CHECKPOINT() BOOST_CHECK_EQUAL(2,2);

#define CHECK_MD5_EXCEPTION(statement) try { \
        statement ; \
        BOOST_FAIL("Should have thrown."); \
    } catch(const Md5FinalizedException & je) { \
    }


BOOST_AUTO_TEST_CASE(identical_strings_should_test_equal)
{
    string a, b;

    {
        Md5 md5;
        md5.update("HELLO!", 6);
        a = md5.finalize();
    }

    {
        Md5 md5;
        md5.update("HELLO!", 6);
        b = md5.finalize();
    }

    BOOST_CHECK_EQUAL(a, b);
}

BOOST_AUTO_TEST_CASE(should_match_system_call)
{
    string a;

    {
        Md5 md5;
        md5.update("HELLO_WORLD", 11);
        a = md5.finalize();
    }
    // output of "md5 -q -s HELLO_WORLD"
    BOOST_CHECK_EQUAL(a, "eba8edba90d6aabf1435c5843586682e");
}


BOOST_AUTO_TEST_CASE(different_input_should_test_not_equal)
{
    string a, b;

    {
        Md5 md5;
        md5.update("HELLO!", 6);
        a = md5.finalize();
    }

    {
        Md5 md5;
        md5.update("GOOD-BYE!", 9);
        b = md5.finalize();
    }

    BOOST_CHECK(a != b);
}


BOOST_AUTO_TEST_CASE(md5_update_after_finalization)
{
    Md5 md5;
    md5.update("HELLO", 5);
    md5.finalize();
    CHECK_MD5_EXCEPTION({
        md5.update("CRASH", 5);
    });
    CHECK_MD5_EXCEPTION({
        md5.finalize();
    });

}

BOOST_AUTO_TEST_CASE(md5_multiple_updates)
{
    string a, b;

    string x = "HELLO";
    string y = "WORLD";
    string z = x + y;

    Md5 md5_parts, md5_whole;

    md5_parts.update(x.c_str(), x.size());
    md5_parts.update(y.c_str(), y.size());
    a = md5_parts.finalize();

    md5_whole.update(z.c_str(), z.size());
    b = md5_whole.finalize();

    BOOST_CHECK_EQUAL(a, b);

}
