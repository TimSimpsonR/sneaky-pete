#ifndef __NOVA_GUEST_UTILS_H
#define __NOVA_GUEST_UTILS_H

#include <string>


namespace nova { namespace guest { namespace utils {

std::string get_host_name();

std::string get_ipv4_address(const char * device_name);

}}} // end nova::guest::utils

#endif
