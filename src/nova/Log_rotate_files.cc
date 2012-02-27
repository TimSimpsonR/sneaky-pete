#include "nova/Log.h"

#include <boost/format.hpp>
#include "nova/utils/io.h"
#include <string>

using boost::format;
namespace io = nova::utils::io;
using std::string;

namespace nova {

namespace {

    class LogFileRotater {
    public:

        LogFileOptions options;

        LogFileRotater(LogFileOptions options) : options(options) {
        }

        inline string file_path(int index) {
            if (index > 0) {
                return str(format("%s.%d") % options.path % index);
            } else {
                return options.path;
            }
        }

        /** Returns the index ending the last file given the prefix file path and the
         *  last possible index number if one was found. */
        int oldest_log_file_index() {
            for (int i = options.max_old_files; i > 0; i --) {
                string possible_file = file_path(i);
                if (io::is_file_sans_logging(possible_file.c_str())) {
                    return i;
                }
            }
            return 0;
        }

        void rotate() {
            for (int i = oldest_log_file_index(); i >= 0; i --) {
                string old_file = file_path(i);
                if (io::is_file_sans_logging(old_file.c_str())) {
                    if (i == options.max_old_files) {
                        ::remove(old_file.c_str());
                    } else {
                        string new_file = file_path(i + 1);
                        ::rename(old_file.c_str(), new_file.c_str());
                    }
                } else {
                    // Just skip it...
                }
            }
        }

    };

} // end anonymous namespace


void Log::_rotate_files(LogFileOptions options) {
    LogFileRotater rotater(options);
    rotater.rotate();
}

} // end nova namespace
