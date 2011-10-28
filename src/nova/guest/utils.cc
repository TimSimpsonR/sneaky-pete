#include "nova/guest/utils.h"

#include <arpa/inet.h>
//#include <fcntl.h>
#include "nova/guest/guest_exception.h"
//#include <netinet/in.h>
//#include <sys/socket.h>
//#include <stropts.h>
#include <string.h>
#include <sys/types.h>
#include <ifaddrs.h>


using nova::guest::GuestException;
using std::string;

namespace nova { namespace guest { namespace utils {

string get_ipv4_address(const char * device_name) {
    char buf[128];
    strncpy(buf, "hi", 2);
    string rtn;

    ifaddrs * interfaces;
    int result = getifaddrs(&interfaces);
    if (result == -1) {
        throw GuestException(GuestException::COULD_NOT_GET_INTERFACES);
    }
    for(ifaddrs * itr = interfaces; itr != NULL; itr = itr->ifa_next) {
        if (strcmp(device_name, itr->ifa_name) == 0) {
            sockaddr * address = itr->ifa_addr;
            if (inet_ntop(AF_INET, address, buf, 128) == 0) {
                throw GuestException(GuestException::COULD_NOT_CONVERT_ADDRESS);
            }
            rtn = buf;
        }
    };

    freeifaddrs(interfaces);

    return rtn;
}


}}} // end nova::guest::utils
