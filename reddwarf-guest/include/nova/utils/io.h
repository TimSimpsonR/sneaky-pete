#ifndef _NOVA_UTILS_IO_H
#define _NOVA_UTILS_IO_H

#include <boost/optional.hpp>
#include <sys/select.h>
#include <signal.h>
#include <time.h>

namespace nova { namespace utils { namespace io {

/**
 * Create this to cause select_with_throw to throw TimeOutExceptions.
 */
class Timer {
    public:
        Timer(double seconds);

        ~Timer();

        static void interrupt(int signal_number, siginfo_t * info,
                              void * context);

        static void remove_interrupt_handler();

        static void set_interrupt_handler();

        static bool & time_out_occurred();

    private:
        Timer(const Timer &);
        Timer & operator = (const Timer &);

        timer_t id;
};


bool is_file(const char * file_path);

bool is_file_sans_logging(const char * file_path);

/** Throws exceptions if errors are detected. */
size_t read_with_throw(int fd, char * const buf, size_t count);

/** Throws exceptions if errors are detected.
 * Also unblocks time out signals before proceeding and throws
 * ProcessTimeOutExceptions if they happen. */
int select_with_throw(int nfds, fd_set * readfds, fd_set * writefds,
                      fd_set * errorfds, boost::optional<double> seconds);


class IOException : public std::exception {

    public:
        enum Code {
            ACCESS_DENIED,
            GENERAL,
            READ_ERROR,
            SIGNAL_HANDLER_DESTROY_ERROR,
            SIGNAL_HANDLER_INITIALIZE_ERROR,
            TIMER_DISABLE_ERROR,
            TIMER_ENABLE_ERROR
        };

        IOException(Code code) throw();

        virtual ~IOException() throw();

        virtual const char * what() const throw();

    private:
        Code code;
};

class TimeOutException : public std::exception {

    public:
        TimeOutException() throw();

        virtual ~TimeOutException() throw();

        virtual const char * what() const throw();

};

} } } // end nova::utils::io

#endif
