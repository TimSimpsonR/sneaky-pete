#define BOOST_TEST_MODULE ConfigFile_Tests
#include <boost/test/unit_test.hpp>
#include "nova/ConfigFile.h"


using nova::ConfigFile;


BOOST_AUTO_TEST_CASE(test_read_configfile)
{
   // ConfigFile config_file("config/test-configfile.txt");

   // BOOST_CHECK_EQUAL(config_file.get_int("amqp_port"), 5672);
   // BOOST_CHECK_EQUAL(config_file.get_string("amqp_host"), "10.0.4.15");
}
