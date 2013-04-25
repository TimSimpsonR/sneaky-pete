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
const char* FILE_NAME;

struct Segment;

struct Segment {
    Segment * prev;
    size_t size;

    Segment(Segment * p, size_t s)
    :prev(p), size(s)
    {
    }
};

struct Diagnostics
{
    size_t fd_size;
    size_t vm_size;
    size_t vm_peak;
    size_t vm_rss;
    size_t vm_hwm;
    size_t threads;
};

void go_on() {
    std::string unused_input;
    std::cerr << "Hit enter to proceed.";
    std::cin >> unused_input;
}

void write_output(std::string str) {
    std::ofstream myfile;
    myfile.open(FILE_NAME, std::ios_base::app);
    myfile << str;
    myfile.close();
}

Diagnostics get_diags() {
    LogApiScope log(LogOptions::simple());
    Diagnostics diagnostics;
    std::stringstream sstr;
    sstr << "Starting up the Interrogator..." << std::endl;
    Interrogator gator;
    DiagInfoPtr output = gator.get_diagnostics();
    sstr << std::endl << std::endl;
    sstr << "getDiagnostics = " << output.get() << std::endl;
    sstr << "fd_size = " << output->fd_size << std::endl;
    diagnostics.fd_size = output->fd_size;
    sstr << "vm_size = " << output->vm_size << std::endl;
    diagnostics.vm_size = output->vm_size;
    sstr << "vm_peak = " << output->vm_peak << std::endl;
    diagnostics.vm_peak = output->vm_peak;
    sstr << "vm_rss = " << output->vm_rss << std::endl;
    diagnostics.vm_rss = output->vm_rss;
    sstr << "vm_hwm = " << output->vm_hwm << std::endl;
    diagnostics.vm_hwm = output->vm_hwm;
    sstr << "threads = " << output->threads << std::endl;
    diagnostics.threads = output->threads;
    sstr << "version = " << output->version << std::endl;
    write_output(sstr.str());
    return diagnostics;
}

int main(int argc, char* argv[]) {
    std::cerr << "argc: " << argc << std::endl;
    for (int i(0); i<argc; i++) {
        std::cerr << "argv[" << i << "]: " << argv[i] << std::endl;
    }


    if (argc == 1) {
        FILE_NAME = "/tmp/greedy_output.txt";
        std::cerr << "using default file path." << std::endl;

    }
    if (argc == 2) {
        std::cerr << "argv[1]: " << argv[1] << std::endl;
        FILE_NAME = argv[1];
    }

    std::cerr << "Hello. My name is Greedy Pete." << std::endl;
    std::cerr << "My proc id is " << getpid() << " and I liked to eat and eat."
              << std::endl;

    std::ofstream myfile;
    myfile.open(FILE_NAME);
    myfile.close();

    Diagnostics diags_before = get_diags();

    try {
        // go_on();
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
            write_output(sstr.str());


        }

    } catch (std::bad_alloc& ba) {
        std::cerr << "bad_alloc caught: " << ba.what() << std::endl;
    }

    Diagnostics diags_after = get_diags();

    std::stringstream out;
    out << std::endl;
    out << "Differences between start and end of diagnostics" << std::endl;
    out << "fd_size: " << diags_after.fd_size-diags_before.fd_size << std::endl;
    out << "vm_size: " << diags_after.vm_size-diags_before.vm_size << std::endl;
    out << "vm_peak: " << diags_after.vm_peak-diags_before.vm_peak << std::endl;
    out << "vm_rss: "  << diags_after.vm_rss-diags_before.vm_rss << std::endl;
    out << "vm_hwm: "  << diags_after.vm_hwm-diags_before.vm_hwm << std::endl;
    out << "threads: " << diags_after.threads-diags_before.threads << std::endl;

    std::cerr << out.str();
    write_output(out.str());

}
