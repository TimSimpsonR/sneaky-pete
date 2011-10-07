#include "nova/guest/apt.h"

namespace nova { namespace guest {

/**---------------------------------------------------------------------------
 *- AptException
 *---------------------------------------------------------------------------*/

AptException::AptException(Code code) throw()
: code(code) {
}

AptException::~AptException() throw() {
}

const char * AptException::what() const throw() {
    switch(code) {
        case ADMIN_LOCK_ERROR:
            return "Cannot complete because admin files are locked.";/*
        case INIT_CONFIG_FAILED:
            return "Initialization of pkg config failed.";
        case INIT_SYSTEM_FAILED:
            return "Initialization of pkg system failed.";
        case NO_ARCH_INFO:
            return "No architecture information was available for the host "
                   "architecture.";
        case OPEN_FAILED:
            return "Could not open cache!";*/
        case PACKAGE_NOT_FOUND:
            return "Could not find the given package.";
        case PACKAGE_STATE_ERROR:
            return "Package is in a bad state.";
        case PERMISSION_ERROR:
            return "Invalid permissions.";
        case PROCESS_CLOSE_TIME_OUT:
            return "A called process did not finish when predicted.";
        case PROCESS_TIME_OUT:
            return "A called process did not give the expected output when "
                   "predicted.";
        case UNEXPECTED_PROCESS_OUTPUT:
            return "The process returned unexpected output.";
        default:
            return "An error occurred.";
    }
}

} }  // end namespace nova::guest::apt

