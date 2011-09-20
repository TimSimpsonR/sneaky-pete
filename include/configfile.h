#ifndef __NOVAGUEST_CONFIGFILE_H
#define __NOVAGUEST_CONFIGFILE_H

#include "confuse.h"
#include <string>

class Configfile {
public:
    Configfile(const std::string & config_path);
    Configfile();
    ~Configfile();
    std::string get_string(const std::string & key);

private:
    cfg_t *cfg;

};

#endif