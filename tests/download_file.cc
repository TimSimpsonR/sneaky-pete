#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fstream>
#include <iostream>
#include "nova/utils/Curl.h"
#include <boost/format.hpp>
#include "nova/utils/Md5.h"
#include "nova/Log.h"
#include <boost/assign/list_of.hpp>
#include "nova/utils/swift.h"

using namespace std;
using namespace boost;
using namespace boost::assign;
using nova::Log;
using nova::LogApiScope;
using nova::LogOptions;

using nova::utils::Curl;
using nova::utils::CurlScope;

using namespace nova::utils::swift;

/*
 * This example shows a HTTP PUT operation. PUTs a file given as a command
 * line argument to the URL also given on the command line.
 *
 * This example also uses its own read callback.
 *
 * Here's an article on how to setup a PUT handler for Apache:
 * http://www.apacheweek.com/features/put
 */


class FileWriter : public SwiftDownloader::Output {
public:

    FileWriter(const char * file_name)
    :   file(file_name, std::ofstream::binary)
    {
    }

    virtual ~FileWriter() {
    }

    virtual void write(const char * buffer, size_t buffer_size) {
        file.write(buffer, buffer_size);
    }

private:
    bool _eof;
    ofstream file;
};


int main(int argc, char **argv)
{
    CurlScope scope;
    LogApiScope log(LogOptions::simple());

    if (argc < 6) {
        const char * program_name = (argc > 0 ? argv[0] : "download_file");
        NOVA_LOG_ERROR("Usage: %s  output_file token base_url container "
                        "base_file_name", program_name);
      return 1;
    }

    const auto output_file_name = argv[1];
    const auto token = argv[2];

    SwiftFileInfo swift_file;
    swift_file.base_url = argv[3];
    swift_file.container = argv[4];
    swift_file.base_file_name = argv[5];

    NOVA_LOG_DEBUG("Writing output file.")

    FileWriter local_file(output_file_name);


    //const auto max_bytes = 32 * 1024;
    //const auto chunk_size = 16 * 1024; // <-- make believe LOL
    SwiftDownloader client(token, swift_file);

    client.read(local_file);

    return 0;
}
