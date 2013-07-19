#ifndef _NOVA_UTILS_IO_H
#define _NOVA_UTILS_IO_H

#include <boost/optional.hpp>
#include <sys/select.h>
#include <signal.h>
#include <time.h>
#include <boost/utility.hpp>

/**
 *  These functions translate the RAII semantics of Sneaky Pete's code to
 *  the signal handling semantics of these low level functions. They are
 *  necessary so that we don't have to check weird error codes and account
 *  for static variables and signal masks all over Sneaky Pete. Instead, the
 *  details of how that works lives here.
 *
 *  For example, many of these functions may fail if an interrupt signal
 *  is given. However, the expectation is unless this code created an interrupt
 *  for an alarm (in which case a TimeOutException is thrown) the function
 *  should just be called again. These functions do that automatically. If
 *  the calls fail due to an error other than an interrupt, these functions
 *  will also log the error and throw an exception.
 */
namespace nova { namespace utils { namespace io {


/** Helper class for managing the file descriptors of a pipe.
 *  It can be hard to remember to close both ends, so this class keeps track
 *  of that to ensure resources aren't leaked. */
class Pipe : private boost::noncopyable
{
    public:
        enum {
            IN = 0,
            OUT = 1
        };
        Pipe();

        ~Pipe();

        void close_in();

        void close_out();

        inline int in() {
            return fd[IN];
        }

        inline bool in_is_open() {
            return is_open[IN];
        }

        inline int out() {
            return fd[OUT];
        }

        inline bool out_is_open() {
            return is_open[OUT];
        }

    private:

        int fd[2];
        bool is_open[2];

        void close(int index);
};

/**
 * Create this to cause the functions here to throw TimeOutExceptions.
 * Only one of these can be used at a time. Unexpected behavior may
 * occur if multiple timers are used at once.
 */
class Timer : boost::noncopyable {
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

bool is_directory(const char * directory_path);

bool is_file(const char * file_path);

bool is_file_sans_logging(const char * file_path);

/** Throws exceptions if errors are detected. */
size_t read_with_throw(int fd, char * const buf, size_t count);

/** Throws exceptions if errors are detected.
 * Also unblocks time out signals before proceeding and throws
 * ProcessTimeOutExceptions if they happen. */
int select_with_throw(int nfds, fd_set * readfds, fd_set * writefds,
                      fd_set * errorfds, boost::optional<double> seconds);


/** Calls the waitpid function, but automatically retries in the event
 *  of an interrupt and throws TimeOutException if a time out occurs. */
int wait_pid_with_throw(pid_t pid, int * status, int options);

class IOException : public std::exception {

    public:
        enum Code {
            ACCESS_DENIED,
            GENERAL,
            PIPE_CREATION_ERROR,
            READ_ERROR,
            SIGNAL_HANDLER_DESTROY_ERROR,
            SIGNAL_HANDLER_INITIALIZE_ERROR,
            TIMER_DISABLE_ERROR,
            TIMER_ENABLE_ERROR,
            WAITPID_ERROR
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
