#include "nova/guest/utils.h"
#include "nova/flags.h"
#include <iostream>
#include <string>

using namespace nova::flags;
using namespace std;

// This is just a test program to print out the attributes the guest should
// autodetect on start-up.
int main(int argc, char* argv[]) {
    FlagMapPtr flags = FlagMap::create_from_args(argc, argv, true);

    const char * guest_ethernet_device = flags->get("", "eth0");

    cout << "Host Name = " << nova::guest::utils::get_host_name() << endl;

    string value = nova::guest::utils::get_ipv4_address(guest_ethernet_device);
    cout << "Ip V4 Address = " << value << endl;

}
