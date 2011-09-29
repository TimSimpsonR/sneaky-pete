#ifndef __NOVA_GUEST_APT_H
#define __NOVA_GUEST_APT_H

#include "guest.h"

namespace nova { namespace guest {

    namespace apt {
        void install(const char * package_name, int time_out);

        void remove(const char * package_name, int time_out);

        std::string version(const char * package_name);
    }

    class AptException : public std::exception {

        public:
            enum Code {
                ADMIN_LOCK_ERROR,
            };

            AptException(Code code) throw();

            virtual ~AptException() throw();

            virtual const char * what() const throw();

        private:
            Code code;

    };


    class AptMessageHandler : public MessageHandler {

        public:
          AptMessageHandler();

          virtual nova::JsonObjectPtr handle_message(nova::JsonObjectPtr input);
    };

} }  // end namespace

#endif //__NOVA_GUEST_APT_H
