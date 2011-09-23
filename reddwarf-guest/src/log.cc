#include "log.h"

#include <iostream>
#ifdef _DEBUG
#include <syslog.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#endif

void Log::info(const std::string & msg) {
    #ifdef _DEBUG
        std::cout << msg << std::endl;
    #endif
    syslog(LOG_INFO, "%s", msg.c_str());
}

void Log::info2(const char* format, ... ) {
    
    va_list args;
    va_start(args, format);
    const int BUFF_SIZE = 1024;
    char buf[BUFF_SIZE];
    int read = vsnprintf(buf, BUFF_SIZE, format, args);

    #ifdef _DEBUG
        std::cout << buf << std::endl;
    #endif
    
    syslog(LOG_INFO, "%s", buf);
    va_end(args);
}


void Log::error(const std::string & msg) {
    #ifdef _DEBUG
        std::cerr << msg << std::endl;
    #endif
    syslog(LOG_ERR, "%s", msg.c_str());
}
