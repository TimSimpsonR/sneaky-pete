#ifndef __NOVA_LOG_H
#define __NOVA_LOG_H

#include <string>

namespace nova {

    class Log {

        public:
            void debug(const char* format, ... );
            void info(const std::string & msg);
            void info2(const char* format, ... );
            void error(const std::string & msg);
            void error2(const char* format, ... );

    };

}

#endif

