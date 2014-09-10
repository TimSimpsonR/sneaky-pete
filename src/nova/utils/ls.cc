#include "ls.h"
#include <sstream>
#include "nova/process.h"
#include <boost/assign/list_of.hpp>

using namespace boost::assign;
using namespace nova::process;
using std::string;
using std::stringstream;
using std::vector;

namespace nova { namespace utils {

    void ls(const string & directory, vector<string> & output) {
        stringstream stdout;
        const CommandList cmds = list_of("/usr/bin/sudo")("-E")("/bin/ls")
                                        (directory.c_str());
        execute(stdout, cmds, 60.0);
        while(stdout.good()) {
            string item;
            stdout >> item;
            output.push_back(item);
        }
    }

} }
