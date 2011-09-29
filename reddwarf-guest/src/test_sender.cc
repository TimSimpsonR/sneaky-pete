#include "nova/guest/guest.h"
#include "nova/rpc/sender.h"
#include "nova/rpc/amqp.h"
#include <iostream>
#include <sstream>
#include <unistd.h>


using namespace nova;
using namespace nova::guest;
using namespace nova::rpc;


int main(int argc, const char* argv[]) {
	if (argc < 2) {
		std::cerr << "Please specify the method name as the first argument."
		          << std::endl;
		return 1;
	}
	const char * method_name = argv[1];
    try {
        // daemon(1,0);
        Sender sender("localhost", 5672, "guest", "guest",
                      "guest.hostname_exchange", "guest.hostname", "");
        std::stringstream publish_string;
        publish_string << "{'method': '" << method_name << "'}";
        std::cout << publish_string.str() << std::endl;
        sender.send(publish_string.str().c_str());
    } catch (std::exception e) {
        std::cout << "Error : " << e.what() << std::endl;
    }
    return 0;
}
