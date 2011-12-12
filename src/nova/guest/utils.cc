#include "nova/guest/utils.h"

#include <arpa/inet.h>
//#include <fcntl.h>
#include "nova/guest/GuestException.h"
//#include <netinet/in.h>
//#include <sys/socket.h>
//#include <stropts.h>
#include "nova/Log.h"
#include <string.h>
#include <sys/types.h>
#include <ifaddrs.h>
#include <sys/socket.h>


using nova::guest::GuestException;
using std::string;

namespace nova { namespace guest { namespace utils {

namespace {

    void set_time(char * buffer, size_t buffer_size, const char * format) {
        time_t t = time(NULL);
        // The returned pointer is statically allocated and shared, so
        // don't free it.
        tm * tmp = localtime(&t);
        if (tmp == NULL) {
            NOVA_LOG_ERROR("Could not get localtime!");
            throw GuestException(GuestException::GENERAL);
        }
        if (strftime(buffer, buffer_size, format, tmp) == 0) {
            NOVA_LOG_ERROR("strftime returned 0");
            throw GuestException(GuestException::GENERAL);
        }
    }

}

string get_host_name() {
    char buf[256];
    if (gethostname(buf, 256) < 0) {
        throw GuestException(GuestException::ERROR_GRABBING_HOST_NAME);
    }
    return string(buf);
}


string get_ipv4_address(const char * device_name) {
    char buf[INET_ADDRSTRLEN];
    strncpy(buf, "hi", 2);
    string rtn;

    ifaddrs * interfaces;
    int result = getifaddrs(&interfaces);
    if (result == -1) {
        throw GuestException(GuestException::COULD_NOT_GET_INTERFACES);
    }
    for(ifaddrs * itr = interfaces; itr != NULL; itr = itr->ifa_next) {
        if (itr->ifa_addr != 0
            && itr->ifa_addr->sa_family == AF_INET /* == ipv4 */
            && strcmp(device_name, itr->ifa_name) == 0) {
            sockaddr_in * in_address = (struct sockaddr_in *)itr->ifa_addr;
            void * address = &(in_address->sin_addr);
            if (inet_ntop(AF_INET, address, buf, INET_ADDRSTRLEN) == 0) {
                freeifaddrs(interfaces);
                throw GuestException(GuestException::COULD_NOT_CONVERT_ADDRESS);
            }
            rtn = buf;
        }
    };

    freeifaddrs(interfaces);

    return rtn;
}


IsoTime::IsoTime() {
    set_time(str, sizeof(str), "%Y-%m-%d");
}

const char * IsoTime::c_str() const {
    return str;
}

IsoDateTime::IsoDateTime() {
    set_time(str, sizeof(str), "%Y-%m-%d %H:%M:%S");
}

const char * IsoDateTime::c_str() const {
    return str;
}


}}} // end nova::guest::utils
