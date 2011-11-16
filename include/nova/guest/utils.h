#ifndef __NOVA_GUEST_UTILS_H
#define __NOVA_GUEST_UTILS_H

#include <string>


namespace nova { namespace guest { namespace utils {

std::string get_host_name();

std::string get_ipv4_address(const char * device_name);


class IsoTime {
    public:
        IsoTime();

        const char * c_str() const;

    private:
        char str[11];
};

class IsoDateTime {
    public:
        IsoDateTime();

        const char * c_str() const;

    private:
        char str[20];
};


}}} // end nova::guest::utils

#endif
