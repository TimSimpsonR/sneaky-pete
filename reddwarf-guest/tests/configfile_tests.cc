#define BOOST_TEST_MODULE Configfile_Tests
#include <boost/test/unit_test.hpp>
#include "confuse.h"


BOOST_AUTO_TEST_CASE(test_read_configfile)
{
    cfg_t *cfg;
    cfg_opt_t opts[] = {
        CFG_STR("amqp_uri", "guest:guest@localhost:5672/", CFGF_NONE),
        CFG_STR("amqp_queue", "guest.hostname", CFGF_NONE),
        CFG_END()
    };
    cfg = cfg_init(opts, CFGF_NOCASE);
    BOOST_REQUIRE_EQUAL(cfg_parse(cfg, "config/test-configfile.txt"), CFG_SUCCESS);
    printf("uri is %s\n", cfg_getstr(cfg, "amqp_uri"));
    printf("queue is %s\n", cfg_getstr(cfg, "amqp_queue"));
    cfg_free(cfg);
}
