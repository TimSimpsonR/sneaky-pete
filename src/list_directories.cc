#include <boost/foreach.hpp>
#include <nova/log.h>
#include <iostream>
#include <nova/utils/io.h>


using namespace std;
using nova::LogApiScope;
using nova::LogOptions;
using namespace nova::utils::io;


int main(int argc, char* argv[]) {
    LogApiScope log(LogOptions::simple());

    if (argc < 2) {
        cout << "Usage: " << (argc < 1 ? "list_directories" : argv[0])
                          << " directory" << endl;
        return 10;
    }

    Directory directories(argv[1]);
    BOOST_FOREACH(const std::string & file, directories.get_files()) {
        cout << file << endl;
    }
    return 0;
}
