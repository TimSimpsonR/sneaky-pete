#include <AMQPcpp.h>
#include "sender.h"
#include <sstream>
#include <unistd.h>


int main(int argc, const char* argv[]) {
	if (argc < 2) {
		std::cerr << "Please specify the method name as the first argument."
		          << std::endl;
		return 1;
	}
	const char * method_name = argv[1];
    try {
        // daemon(1,0);
        Sender sender("guest:guest@localhost:5672/", "guest.hostname_exchange",
                      "guest.hostname");
        std::stringstream publish_string;
        publish_string << "{'method': '" << method_name << "'}";
        cout << publish_string.str() <<endl;
        sender.send(publish_string.str().c_str());
    } catch (AMQPException e) {
        std::cout << "An exception occured (code " << e.getReplyCode()
                  << "):" << e.getMessage() << std::endl;
    }
    return 0;
}
