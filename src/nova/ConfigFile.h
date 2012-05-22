#ifndef __NOVA_CONFIGFILE_H
#define __NOVA_CONFIGFILE_H

#include "confuse.h"
#include <string>

namespace nova {

    class ConfigFile {
    public:
        ConfigFile(const char * config_path);
        ~ConfigFile();
        int get_int(const std::string & key);
        std::string get_string(const std::string & key);
    private:
        ConfigFile(const ConfigFile &);
        ConfigFile & operator = (const ConfigFile &);

        cfg_t *cfg;

    };

}

#endif
