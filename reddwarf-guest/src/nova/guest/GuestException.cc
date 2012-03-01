#include "nova/guest/GuestException.h"


namespace nova { namespace guest {

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
        case COULD_NOT_CONVERT_ADDRESS:
            return "Could not convert the address to a string.";
        case COULD_NOT_GET_DEVICE:
            return "Could not grab info on the given device.";
        case COULD_NOT_GET_INTERFACES:
            return "Could not get the network interfaces.";
        case ERROR_GRABBING_HOST_NAME:
            return "Error grabbing the host name.";
        case MALFORMED_INPUT:
            return "The input message was malformed.";
        case NO_SUCH_METHOD:
            return "Could not handle the JSON input. No such method was found.";
        default:
            return "An error occurred.";
    }
}

} }  // end namespace
