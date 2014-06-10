#include "pch.hpp"
#include "nova/guest/diagnostics.h"
#include "nova/Log.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <unistd.h>
#include <exception>

using namespace nova;
using namespace nova::guest::diagnostics;

const size_t MB = 1024 * 1024;


struct Segment {
    Segment * prev;
    size_t size;

    Segment(Segment * p, size_t s)
    :prev(p), size(s)
    {
    }
};

void write_diag_diff(const char * file_name,
                     const DiagInfoPtr & start_diags,
                     const DiagInfoPtr & end_diags) {
  auto diff = (*end_diags) - (*start_diags);

  std::stringstream out;
  out << std::endl;

  out << "{ " << std::endl;
  out << "  \"fd_size\":" << diff.fd_size << ", " << std::endl;
  out << "  \"vm_size\":" << diff.vm_size << ", " << std::endl;
  out << "  \"vm_peak\":" << diff.vm_peak << ", " << std::endl;
  out << "   \"vm_rss\":" << diff.vm_rss << ", " << std::endl;
  out << "   \"vm_hwm\":" << diff.vm_hwm << ", " << std::endl;
  out << "  \"threads\":" << diff.threads << std::endl;
  out << "}" << std::endl;

  std::ofstream myfile;
  myfile.open(file_name);
  myfile << out.str();
  myfile.close();
}


void max_out(const DiagInfoPtr start_diags, const char * file_name) {
    try {
        size_t sum = 0;
        Segment * current = 0;
        while (sum <= MB * 8048) {
            size_t size = MB;
            void * next = ::operator new(size);
            char * chars = reinterpret_cast<char *>(next); //::operator new(size);
            for (size_t i = 0; i < size; i ++) {
                chars[i] = 'a';
            }
            Segment * new_segment = new (next) Segment(current, size);
            current = new_segment;
            sum += size;

            std::stringstream sstr;
            sstr << "Current = " << current << ", Sum = " << sum
                << " (" << (sum / MB) << " MB)" << std::endl;
            std::cerr << sstr.str();

            // We write (_not_ append)  the diag differnece every time since
            // we can receive a kill 9 signal which we can't handle
            Interrogator interrogator("/");
            DiagInfoPtr diags_now = interrogator.get_diagnostics();
            write_diag_diff(file_name, start_diags, diags_now);
        }

    } catch (std::bad_alloc& ba) {
        std::cerr << "bad_alloc caught: " << ba.what() << std::endl;
    }

}


int main(int argc, char* argv[]) {
    // Logs are disabled since interrogator logs every get_diagnostics call
    // which is being called many times.
    LogApiScope log(LogOptions::silent());

    if (argc > 2) {
        std::cerr << "Usage: " << argv[0] << " [outputfile]" << std::endl;
        return 1;
    }
    const char * file_name = argc == 1 ? "/tmp/greedy_output.txt" : argv[1];


    std::cerr << "Hello. My name is Greedy Pete." << std::endl;
    std::cerr << "My proc id is " << getpid() << " and I liked to eat and eat."
              << std::endl;

    Interrogator interrogator("/");
    DiagInfoPtr diags_start = interrogator.get_diagnostics();

    max_out(diags_start, file_name);
}
