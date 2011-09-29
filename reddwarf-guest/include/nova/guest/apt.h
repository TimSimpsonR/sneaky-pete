#ifndef __NOVA_GUEST_APT_H
#define __NOVA_GUEST_APT_H

#include "guest.h"
#include <boost/optional.hpp>


namespace nova { namespace guest {

    namespace apt {
        void fix(double time_out);

        void install(const char * package_name, const double time_out);

        void remove(const char * package_name, const double time_out);

        boost::optional<std::string> version(const char * package_name,
                                             const double time_out=30);
    }

    class AptException : public std::exception {

        public:
            enum Code {
                /*ADMIN_LOCK_ERROR,
                GENERAL,
                INIT_CONFIG_FAILED,
                INIT_SYSTEM_FAILED,
                NO_ARCH_INFO,
                OPEN_FAILED*/
                ADMIN_LOCK_ERROR,
                COULD_NOT_FIX,
                GENERAL,
                PACKAGE_NOT_FOUND,
                PACKAGE_STATE_ERROR,
                PERMISSION_ERROR,
                PROCESS_CLOSE_TIME_OUT,
                PROCESS_TIME_OUT,
                UNEXPECTED_PROCESS_OUTPUT

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

} }  // end namespace

#endif //__NOVA_GUEST_APT_H
