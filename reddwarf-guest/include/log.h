#ifndef __NOVAGUEST_LOG_H
#define __NOVAGUEST_LOG_H

#include <string>

class Log {

    public:
        void info(const std::string & msg);
        void error(const std::string & msg);

};

#endif

