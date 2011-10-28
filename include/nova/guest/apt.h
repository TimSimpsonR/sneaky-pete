#ifndef __NOVA_GUEST_APT_H
#define __NOVA_GUEST_APT_H

#include "guest.h"
#include <boost/optional.hpp>


namespace nova { namespace guest { namespace apt {

    void fix(double time_out);

    void install(const char * package_name, const double time_out);

    void remove(const char * package_name, const double time_out);

    boost::optional<std::string> version(const char * package_name,
                                         const double time_out=30);


    class AptException : public std::exception {

        public:
            enum Code {
                /*ADMIN_LOCK_ERROR,
                GENERAL,
                INIT_CONFIG_FAILED,
                INIT_SYSTEM_FAILED,
                NO_ARCH_INFO,
                OPEN_FAILED*/
                ADMIN_LOCK_ERROR = 0,
                COULD_NOT_FIX = 10,
                GENERAL = 20,
                PACKAGE_NOT_FOUND = 30,
                PACKAGE_STATE_ERROR = 40,
                PERMISSION_ERROR = 50,
                PROCESS_CLOSE_TIME_OUT = 60,
                PROCESS_TIME_OUT = 70,
                UNEXPECTED_PROCESS_OUTPUT = 80

            };

            AptException(const Code code) throw();

            virtual ~AptException() throw();

            virtual const char * what() const throw();

            const Code code;

    };


    class AptMessageHandler : public MessageHandler {

        public:
          AptMessageHandler();

          virtual nova::JsonObjectPtr handle_message(nova::JsonObjectPtr input);
    };

} } }  // end namespace

#endif //__NOVA_GUEST_APT_H
