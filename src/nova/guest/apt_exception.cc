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
            return "Cannot complete because admin files are locked.";
        default:
            return "An error occurred.";
    }
}

} }  // end namespace nova::guest::apt

