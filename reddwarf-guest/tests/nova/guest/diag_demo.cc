#include "nova/guest/diagnostics.h"
#include "nova/Log.h"
#include <iostream>
#include <string>

using namespace nova;
using namespace nova::guest::diagnostics;
using namespace std;

// This is just a test program to print out the attributes the guest should
// autodetect on start-up.
int main(int argc, char* argv[]) {

    LogApiScope log(LogOptions::simple());

    cout << "Starting up the Interrogator..." << endl;

    Interrogator gator;
    DiagInfoPtr output = gator.get_diagnostics();
    cout << endl << endl;
    cout << "getDiagnostics = " << output.get() << endl;
    cout << "fd_size = " << output->fd_size << endl;
    cout << "vm_size = " << output->vm_size << endl;
    cout << "vm_peak = " << output->vm_peak << endl;
    cout << "vm_rss = " << output->vm_rss << endl;
    cout << "vm_hwm = " << output->vm_hwm << endl;
    cout << "threads = " << output->threads << endl;
    cout << "version = " << output->version << endl;

}
