#include "nova/guest/apt.h"
#include <iostream>

using std::cout;
using boost::optional;
using namespace nova;
using namespace nova::guest;
using std::string;
using std::stringstream;


int main(int argc, const char* argv[]) {
    //apt::fix(60);
//    apt::install(argv[1], 30);

    apt::remove("fake_package", 60);
//optional<string> version = apt::version("rabbitmq-server");

    /*for (int i = 0; i < 15; i ++) {
        optional<string> version = apt::version("cowsay");
        if (version) {
            std::cout << "COWSAY VERSION IS " << version << std::endl;
        } else {
            std::cout << "The answer is NOTHING!" << std::endl;
        }
        //(!version, true);
    }
    {
        optional<string> version = apt::version("ghsdfhbsd");
        if (version) {
            std::cout << version;
        }
    }
    {
        optional<string> version = apt::version("figlet");
        if (version) {
            std::cout << "figlet VERSION IS " << version.get() << std::endl;
        } else {
            std::cout << "figlet: The answer is NOTHING!" << std::endl;
        }
    }*/
}
