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
    try {
        // daemon(1,0);
        const char * const queue_name = "6140cf9cf3164e229ef39cb1b0bd1287";
        const char * const exchange_name = "6140cf9cf3164e229ef39cb1b0bd1287";
        const char * const routing_key = "6140cf9cf3164e229ef39cb1b0bd1287";
        Sender sender("10.0.4.15", 5672, "guest", "guest",
                      /* exchange key */ exchange_name,
                      /* queue */ queue_name,
                      /* routing key */ routing_key);
        std::stringstream publish_string;
        //publish_string << "{'method': '" << method_name << "'}";
        publish_string << "{'result': null }";
        std::cout << publish_string.str() << std::endl;
        sender.send(publish_string.str().c_str());
    } catch (const std::exception & e) {
        std::cout << "Error : " << e.what() << std::endl;
    }
    return 0;
}
