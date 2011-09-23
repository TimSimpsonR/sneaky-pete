#ifndef __NOVAGUEST_GUEST_EXCEPTION
#define __NOVAGUEST_GUEST_EXCEPTION

#include <exception>

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

#endif

