#include "nova/guest/diagnostics.h"

namespace nova { namespace guest { namespace diagnostics {

/**---------------------------------------------------------------------------
 *- InterrogatorException
 *---------------------------------------------------------------------------*/

InterrogatorException::InterrogatorException(Code code) throw()
: code(code) {
}

InterrogatorException::~InterrogatorException() throw() {
}

const char * InterrogatorException::what() const throw() {
    switch(code) {
        case FILE_NOT_FOUND:
            return "Status file was not found.";
        case PATTERN_DOES_NOT_MATCH:
            return "Pattern did not match the status line.";
        default:
            return "An error occurred.";
    }
}

} } }  // end namespace nova::guest::diagnostics

