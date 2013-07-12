#include "pch.hpp"
#include "nova/guest/monitoring/monitoring.h"

namespace nova { namespace guest { namespace monitoring {

/**---------------------------------------------------------------------------
 *- MonitoringException
 *---------------------------------------------------------------------------*/

MonitoringException::MonitoringException(Code code) throw()
: code(code) {
}

MonitoringException::~MonitoringException() throw() {
}

const char * MonitoringException::what() const throw() {
    switch(code) {
        case FILE_NOT_FOUND:
            return "Status file was not found.";
        case PATTERN_DOES_NOT_MATCH:
            return "Pattern did not match the status line.";
        case FILESYSTEM_NOT_FOUND:
            return "The specified filesystem could not be found.";
        case CANT_WRITE_CONFIG_FILE:
            return "The config file could not be opened or written.";
        case ERROR_STOPPING_AGENT:
            return "Problem stopping the monitoring agent.";
        default:
            return "An error occurred.";
    }
}

} } }  // end namespace nova::guest::monitoring

