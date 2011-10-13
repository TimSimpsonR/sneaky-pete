#define BOOST_TEST_MODULE Configfile_Tests
#include <boost/test/unit_test.hpp>
#include "nova/configfile.h"


using nova::Configfile;


BOOST_AUTO_TEST_CASE(test_read_configfile)
{
    Configfile config_file("config/test-configfile.txt");

    BOOST_CHECK_EQUAL(config_file.get_int("amqp_port"), 5672);
    BOOST_CHECK_EQUAL(config_file.get_string("amqp_host"), "localhost");
}
