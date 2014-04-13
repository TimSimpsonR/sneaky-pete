#ifndef CONTROL_H
#define CONTROL_H

#include <string>

namespace nova { namespace redis {


class Control
{
    public:
        Control(std::string pid_file);

        int stop();

        int start();

        int disable();

        int enable();

        int install(std::string pkg_name);

        int uninstall(std::string pkg_name);

        int get_pid();
    private:
        std::string _pid_file;

        int _get_pid();
};


}}


#endif /* CONTROL_H */
