#include "guest_exception.h"

GuestException::GuestException(GuestException::Code code) throw()
: code(code)
{
}

GuestException::~GuestException() throw() {
}

const char * GuestException::what() throw() {
    switch(code) {
        case CONFIG_FILE_PARSE_ERROR:
            return "Error parsing config file!";
        default:
            return "An error occurred.";
    }
}
