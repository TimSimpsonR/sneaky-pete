#include "log.h"

#include <iostream>
#ifdef _DEBUG
#include <syslog.h>
#endif

void Log::info(const std::string & msg) {
    #ifdef _DEBUG
        std::cout << msg << std::endl;
    #endif
    syslog(LOG_INFO, "%s", msg.c_str());
}

void Log::error(const std::string & msg) {
    #ifdef _DEBUG
        std::cerr << msg << std::endl;
    #endif
    syslog(LOG_ERR, "%s", msg.c_str());
}
