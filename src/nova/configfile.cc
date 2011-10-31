#include "nova/ConfigFile.h"
#include "nova/guest/GuestException.h"


using nova::ConfigFile;
using nova::guest::GuestException;

// Disable GCC's (correct) warning about passing a "const char *"" to a function
// declared as merely "char *".
#pragma GCC diagnostic ignored "-Wwrite-strings"

ConfigFile::ConfigFile(const char * config_path) {
    cfg_opt_t opts[] = {
        CFG_STR("amqp_host", "localhost", CFGF_NONE),
        CFG_INT("amqp_port", 5672, CFGF_NONE),
        CFG_STR("amqp_user_name", "guest", CFGF_NONE),
        CFG_STR("amqp_password", "guest", CFGF_NONE),
        CFG_STR("amqp_queue", "guest.hostname", CFGF_NONE),
        CFG_STR("mysql_uri", "unix:///var/run/mysqld/mysqld.sock", CFGF_NONE),
        CFG_END()
    };
    cfg = cfg_init(opts, CFGF_NOCASE);
    if (cfg_parse(cfg, config_path) != CFG_SUCCESS) {
        throw GuestException(GuestException::CONFIG_FILE_PARSE_ERROR);
    }
}

ConfigFile::~ConfigFile() {
    cfg_free(cfg);
}

int ConfigFile::get_int(const std::string & key) {
    return cfg_getint(cfg, key.c_str());
}

std::string ConfigFile::get_string(const std::string & key) {
    std::string rtn(cfg_getstr(cfg, key.c_str()));
    return rtn;
}
