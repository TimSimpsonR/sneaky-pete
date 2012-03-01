#include "nova/guest/apt.h"

namespace nova { namespace guest { namespace apt {

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
            return "Cannot complete because admin files are locked.";
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
        case UPDATE_FAILED:
            return "Attempt to call apt-get update failed.";
        default:
            return "An error occurred.";
    }
}

} } }  // end namespace nova::guest::apt

