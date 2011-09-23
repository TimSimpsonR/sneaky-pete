#define BOOST_TEST_MODULE Configfile_Tests
#include <boost/test/unit_test.hpp>
#include "log.h"


BOOST_AUTO_TEST_CASE(test_log_info2)
{
    Log log;
    log.info2("this %s a test: %i", "is", 1);
    log.info2("1234567890"
    "1234567890"
    "1234567890"
    "1234567890"
    "1234567890");
    log.info2("this %s a test"
    "A VERY LONG TEST"
    "A VERY LONG TEST"
    "A VERY LONG TEST"
    "A VERY LONG TEST"
    "A VERY LONG TEST"
    "A VERY LONG TEST"
    "A VERY LONG TEST"
    "A VERY LONG TEST"
    "A VERY LONG TEST"
    "A VERY LONG TEST"
    "A VERY LONG TEST"
    "A VERY LONG TEST"
    "A VERY LONG TEST"
    "A VERY LONG TEST"
    "A VERY LONG TEST"
    "A VERY LONG TEST"
    "A VERY LONG TEST"
    ": %i", "is", 1);
}
