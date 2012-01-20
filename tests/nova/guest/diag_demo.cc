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

    Interrogator gator(true);
    double timeout = 30;
    DiagInfoPtr output = gator.get_diagnostics(timeout);

    cout << "getDiagnostics = " << output.get() << endl;

}
