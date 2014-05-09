#ifndef CONTROL_H
#define CONTROL_H

#include <string>

namespace nova { namespace redis {


class Control
{
    public:
        Control();

        int stop();

        int start();

        int disable();

        int enable();

        int get_pid();

        int get_process_status();

};


}}


#endif /* CONTROL_H */
