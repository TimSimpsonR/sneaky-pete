#ifndef __NOVA_GUEST_DIAGNOSTICS_H
#define __NOVA_GUEST_DIAGNOSTICS_H

#include <boost/optional.hpp>
#include "nova/guest/guest.h"
#include <map>
#include <string>

namespace nova { namespace guest { namespace diagnostics {

    typedef std::map<std::string, std::string> DiagInfo;
    typedef std::auto_ptr<DiagInfo> DiagInfoPtr;

    class Interrogator {

        public:
            Interrogator(bool with_sudo);

            DiagInfoPtr get_diagnostics(double time_out) const;

        private:
            Interrogator(const Interrogator &);
            Interrogator & operator = (const Interrogator &);

            bool with_sudo;
    };

    class InterrogatorException : public std::exception {

        public:
            enum Code {
                FILE_NOT_FOUND,
                PATTERN_DOES_NOT_MATCH
            };

            InterrogatorException(const Code code) throw();

            virtual ~InterrogatorException() throw();

            virtual const char * what() const throw();

            const Code code;

    };


    class InterrogatorMessageHandler : public MessageHandler {

        public:
          InterrogatorMessageHandler(const Interrogator & interrogator);

          virtual nova::JsonDataPtr handle_message(const GuestInput & input);

        private:
          InterrogatorMessageHandler(const InterrogatorMessageHandler &);
          InterrogatorMessageHandler & operator = (const InterrogatorMessageHandler &);

          const Interrogator & interrogator;
    };

} } }  // end namespace

#endif //__NOVA_GUEST_DIAGNOSTICS_H
