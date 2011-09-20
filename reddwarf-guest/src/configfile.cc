#include "configfile.h"

Configfile::Configfile(const std::string & config_path) {
    cfg_opt_t opts[] = {
        CFG_STR("amqp_uri", "guest:guest@localhost:5672/", CFGF_NONE),
        CFG_STR("amqp_queue", "guest.hostname", CFGF_NONE),
        CFG_END()
    };
    cfg = cfg_init(opts, CFGF_NOCASE);
    cfg_parse(cfg, config_path.c_str());
}

Configfile::~Configfile() {
    cfg_free(cfg);
}

std::string Configfile::get_string(const std::string & key) {
    return cfg_getstr(cfg, key.c_str());
}