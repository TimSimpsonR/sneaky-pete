#include <iostream>
#include <boost/assign/list_of.hpp>
#include "nova/Log.h"
#include "nova/process.h"
#include <boost/thread.hpp>
#include <stdlib.h>

using namespace nova;
using std::string;
using std::stringstream;
using namespace boost::assign;

/*
 * This is a demo app which repeatedly runs the process
 * /usr/bin/sudo /usr/bin/mysqladmin ping"
 * It was written to check for leaks in file descriptors.
 */
int main(int argc, char* argv[]) {
    LogApiScope log_api_scope(LogOptions::simple());
    int count = 0;
    while(true) {
        try {
            boost::posix_time::seconds time(5);
            boost::this_thread::sleep(time);
            count ++;
            NOVA_LOG_INFO("Pinging mysqladmin...");
            stringstream out;
            Process::execute(out, list_of("/usr/bin/sudo")
                                         ("/usr/bin/mysqladmin")
                                         ("ping"));
            NOVA_LOG_INFO("Ping complete.");
            NOVA_LOG_INFO2("Current count = %d", count);
        } catch(const std::exception & ex) {
            NOVA_LOG_ERROR(ex.what());
        }
    }
}
