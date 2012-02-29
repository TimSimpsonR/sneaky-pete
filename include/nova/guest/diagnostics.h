#ifndef __NOVA_GUEST_DIAGNOSTICS_H
#define __NOVA_GUEST_DIAGNOSTICS_H

#include <boost/optional.hpp>
#include "nova/guest/guest.h"
#include <map>
#include <string>

namespace nova { namespace guest { namespace diagnostics {

    struct ProcStatus {
        int fd_size;
        int vm_size;
        int vm_peak;
        int vm_rss;
        int vm_hwm;
        int threads;
    };

    struct DiagInfo : public ProcStatus {
        std::string version;
    };

    typedef std::auto_ptr<const DiagInfo> DiagInfoPtr;

    class Interrogator {
        public:
            /** Grabs diagnostics for this program. */
            static DiagInfoPtr get_diagnostics();

            /** Grabs process status for the given process id. */
            static void get_proc_status(pid_t pid, ProcStatus & status);
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
