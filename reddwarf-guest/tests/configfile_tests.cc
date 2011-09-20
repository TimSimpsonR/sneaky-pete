#define BOOST_TEST_MODULE Configfile_Tests
#include <boost/test/unit_test.hpp>
#include "confuse.h"


BOOST_AUTO_TEST_CASE(test_read_configfile)
{
    cfg_t *cfg;
    cfg_opt_t opts[] = {
        CFG_STR("amqp_uri", "guest:guest@localhost:5672/", CFGF_NONE),
        CFG_STR("amqp_queue", "guest.hostname", CFGF_NONE),
        CFG_STR("mysql_uri", "unix:///var/run/mysqld/mysqld.sock", CFGF_NONE),
        CFG_STR("mysql_user", "root", CFGF_NONE),
        CFG_STR("mysql_password", "", CFGF_NONE),
        CFG_END()
    };
    cfg = cfg_init(opts, CFGF_NOCASE);
    BOOST_REQUIRE_EQUAL(cfg_parse(cfg, "config/test-configfile.txt"), CFG_SUCCESS);
    cfg_free(cfg);
}
