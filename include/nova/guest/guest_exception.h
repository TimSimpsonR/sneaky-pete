#ifndef __NOVA_GUEST_GUEST_EXCEPTION_H
#define __NOVA_GUEST_GUEST_EXCEPTION_H

#include <exception>

namespace nova { namespace guest {

    // TODO(tim.simpson): This should become the base class of all the other
    //                    exception classes here so we can know they are safe
    //                    to catch.
    class GuestException : public std::exception {

        public:
            enum Code {
                CONFIG_FILE_PARSE_ERROR
            };

            GuestException(Code code) throw();

            virtual ~GuestException() throw();

            virtual const char * what() throw();

        private:
            Code code;

    };

} }

#endif

