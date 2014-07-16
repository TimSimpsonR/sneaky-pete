#include "pch.hpp"
#include "DatastoreException.h"

namespace nova { namespace datastores {

DatastoreException::DatastoreException(Code code) throw()
:   code(code) {
}

DatastoreException::~DatastoreException() throw() {
}

const char * DatastoreException::what() throw() {
    switch(code) {
        case COULD_NOT_START:
            return "Couldn't start datastore!";
        case COULD_NOT_STOP:
            return "Couldn't stop datastore!";
        default:
            return "An error occurred.";
    }
}

} } // end of namespace
