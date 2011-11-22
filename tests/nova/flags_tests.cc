#define BOOST_TEST_MODULE flags_tests
#include <boost/test/unit_test.hpp>
#include <boost/test/unit_test_monitor.hpp>


#include "nova/flags.h"

using namespace nova::flags;
using std::string;


#define CHECKPOINT() BOOST_CHECK_EQUAL(2,2);

#define CHECK_EXCEPTION(statement, ex_code) try { \
        statement ; \
        BOOST_FAIL("Should have thrown."); \
    } catch(const FlagException & fe) { \
        const char * actual = FlagException::code_to_string(FlagException::ex_code); \
        const char * expected = FlagException::code_to_string(fe.code); \
        BOOST_REQUIRE_EQUAL(actual, expected); \
    }

BOOST_AUTO_TEST_CASE(adding_an_arg)
{
    FlagMapPtr flags(new FlagMap());
    flags->add_from_arg("--blah=10");
    BOOST_CHECK_EQUAL(flags->get("blah"), "10");
}

BOOST_AUTO_TEST_CASE(when_no_dashes)
{
    FlagMapPtr flags(new FlagMap());
    CHECK_EXCEPTION({flags->add_from_arg("blah=10");},
                    NO_STARTING_DASHES);
}

BOOST_AUTO_TEST_CASE(when_no_equal_sign)
{
    FlagMapPtr flags(new FlagMap());
    CHECK_EXCEPTION({flags->add_from_arg("--blah  10");},
                    NO_EQUAL_SIGN);
}

BOOST_AUTO_TEST_CASE(accessing_missing_value)
{
    FlagMapPtr flags(new FlagMap());
    flags->add_from_arg("--blah=10");
    CHECK_EXCEPTION({flags->get("blarg");},
                    KEY_NOT_FOUND);
}

BOOST_AUTO_TEST_CASE(adding_an_identical_value_twice_is_ok)
{
    FlagMapPtr flags(new FlagMap());
    for (int i = 0; i < 3; i ++) {
        flags->add_from_arg("--dinosaur=t-rex");
        BOOST_CHECK_EQUAL(flags->get("dinosaur"), "t-rex");
    }
}

BOOST_AUTO_TEST_CASE(adding_the_same_key_with_a_different_value_is_not)
{
    FlagMapPtr flags(new FlagMap());
    flags->add_from_arg("--dinosaur=t-rex");
    CHECK_EXCEPTION({ flags->add_from_arg("--dinosaur=raptor"); },
                    DUPLICATE_FLAG_VALUE);
}
