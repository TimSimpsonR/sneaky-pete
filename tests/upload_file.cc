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


class FileReader : public SwiftUploader::Input {
public:

  	FileReader(const char * file_name)
  	:   _eof(false),
        file(file_name, std::ifstream::binary)
  	{
  	}

  	virtual ~FileReader() {
  	}

    virtual bool eof() const {
        return _eof;
    }

  	virtual size_t read(char * buffer, size_t bytes) {
  		if (file.eof()) {
        _eof = true;
  			return 0;
  		}
  		file.read(buffer, bytes - 1);
      buffer[bytes] = '\0';
      return file.gcount();
  	}

private:
  bool _eof;
  ifstream file;
};


int main(int argc, char **argv)
{
  LogApiScope log(LogOptions::simple());

  if (argc < 6) {
    cerr << "Usage: " << (argc > 0 ? argv[0] : "upload_file")
         << " src_file token base_url container base_file_name" << endl;
    return 1;
  }

  const auto src_file = argv[1];
  const auto token = argv[2];

  SwiftFileInfo file_info;
  file_info.base_url = argv[3];
  file_info.container = argv[4];
  file_info.base_file_name = argv[5];

  FileReader file(src_file);

  CurlScope scope;

  // TODO: make this a argument
  const auto max_bytes = 32 * 1024;
  const int checksum_wait_time = 60;
  SwiftUploader writer(token, max_bytes, file_info, checksum_wait_time);

  writer.write(file);


  return 0;
}
