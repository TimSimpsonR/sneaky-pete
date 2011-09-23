#include "log.h"
#ifdef _DEBUG
    #include <iostream>
#endif
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <syslog.h>


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
    vsnprintf(buf, BUFF_SIZE, format, args);

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

void Log::error2(const char* format, ... ) {
    va_list args;
    va_start(args, format);
    const int BUFF_SIZE = 1024;
    char buf[BUFF_SIZE];
    vsnprintf(buf, BUFF_SIZE, format, args);

    #ifdef _DEBUG
        std::cerr << buf << std::endl;
    #endif

    syslog(LOG_ERR, "%s", buf);
    va_end(args);
}
