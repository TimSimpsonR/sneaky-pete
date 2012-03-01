#include "nova/utils/io.h"

#include <errno.h>
#include "nova/Log.h"
#include <signal.h>
#include <sys/select.h>
#include <stdio.h>
#include <stdlib.h> // exit
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

using nova::utils::io::IOException;
using nova::Log;
using nova::LogPtr;
using boost::optional;

namespace {

    inline void checkGE0(LogPtr & log, const int return_code,
                         IOException::Code code = IOException::GENERAL) {
        if (return_code < 0) {
            NOVA_LOG_ERROR2("System error : %s", strerror(errno));
            throw IOException(code);
        }
    }

    inline void checkEqual0(LogPtr & log, const int return_code,
                            IOException::Code code = IOException::GENERAL) {
        if (return_code != 0) {
            NOVA_LOG_WRITE(log, LEVEL_ERROR)("System error : %s",
                                             strerror(errno));
            throw IOException(code);
        }
    }

    timespec timespec_from_seconds(double seconds) {
        const long BILLION = 1000000000L;
        timespec time;
        time.tv_sec = seconds;
        time.tv_nsec = ((seconds - time.tv_sec) * BILLION);
        if (time.tv_nsec >= BILLION) {
            time.tv_sec ++;
            time.tv_nsec -= BILLION;
        }
        return time;
    }

    bool internal_is_file(const char * file_path, bool log) {
        struct stat buffer;
        if (stat(file_path, &buffer) == 0) {
            return true;
        }
        if (errno == ENOENT || errno == ENOTDIR) {
            return false;
        } else if (errno == EACCES) {
            if (log) {
                NOVA_LOG_ERROR2("Can not access file at path %s.", file_path);
            }
            throw IOException(IOException::ACCESS_DENIED);
        } else  {
            if (log) {
                NOVA_LOG_ERROR2("stat for path %s returned < 0. errno = %d: "
                                "%s\n EINTR=%d", file_path, errno,
                                strerror(errno), EINTR);
            }
            throw IOException(IOException::GENERAL);
        }
    }
}

namespace nova { namespace utils { namespace io {

/**---------------------------------------------------------------------------
 *- IOException
 *---------------------------------------------------------------------------*/

IOException::IOException(Code code) throw()
: code(code) {
}

IOException::~IOException() throw() {
}

const char * IOException::what() const throw() {
    switch(code) {
        case ACCESS_DENIED:
            return "Access denied.";
        case READ_ERROR:
            return "Read error.";
        case SIGNAL_HANDLER_DESTROY_ERROR:
            return "Could not remove signal handler!";
        case SIGNAL_HANDLER_INITIALIZE_ERROR:
            return "Could not initialize signal handler.";
        case TIMER_DISABLE_ERROR:
            return "Error disabling timer!";
        case TIMER_ENABLE_ERROR:
            return "Error enabling timer!";
        default:
            return "An error occurred.";
    }
}


/**---------------------------------------------------------------------------
 *- TimeOutException
 *---------------------------------------------------------------------------*/

TimeOutException::TimeOutException() throw() {
}

TimeOutException::~TimeOutException() throw() {
}

const char * TimeOutException::what() const throw() {
    return "Time out.";
}


/**---------------------------------------------------------------------------
 *- Timer
 *---------------------------------------------------------------------------*/

Timer::Timer(double seconds) {
    set_interrupt_handler();
    // Initialize timer.
    itimerspec value;
    //TODO(tim.simpson): Valgrind thinks id is not initialized.
    // However even if you use memset on it this error will not go away.
    // http://stackoverflow.com/questions/6122594/timer-create-gives-memory-leaks-issue-syscall-param-timer-createevp-points-to
    // ;_;
    LogPtr log = Log::get_instance();
    checkEqual0(log, timer_create(CLOCK_REALTIME, NULL, &id),
                IOException::TIMER_ENABLE_ERROR);
    value.it_value = timespec_from_seconds(seconds);
    value.it_interval = timespec_from_seconds(0.0);  // Not periodic.
    time_out_occurred() = false;
    // 0 for relative time.
    checkEqual0(log, timer_settime(id, 0, &value, NULL),
                IOException::TIMER_ENABLE_ERROR);
}

Timer::~Timer() {
    LogPtr log = Log::get_instance();
    checkEqual0(log, timer_delete(id),
                IOException::TIMER_DISABLE_ERROR);
    time_out_occurred() = false;
    remove_interrupt_handler();
}

void Timer::interrupt(int signal_number, siginfo_t * info,
                             void * context) {
    NOVA_LOG_ERROR("Interruptin'");
    time_out_occurred() = true;
    remove_interrupt_handler();
    //raise(SIGALRM);
    //signal(SIGALRM, SIG_IGN);
}

void Timer::remove_interrupt_handler() {
    // Unblock all signals. The alarm may happen right now.
    sigset_t blocked_signals;
    sigemptyset(&blocked_signals);         /* Block SIGINT */
    sigaddset(&blocked_signals, SIGALRM);
    sigprocmask(SIGALRM, &blocked_signals, NULL);

    // Remove signal handler
    struct sigaction action;
    action.sa_flags = SA_RESETHAND;
    action.sa_sigaction = NULL;
    if ((sigemptyset(&action.sa_mask) == -1) ||
        (sigaction(SIGALRM, &action, NULL) == -1)) {
        throw IOException(IOException::SIGNAL_HANDLER_DESTROY_ERROR);
    }
}

void Timer::set_interrupt_handler() {
    // Block the very signal we're generating so nothing catches it.
    sigset_t blocked_signals;
    sigemptyset(&blocked_signals);         /* Block SIGINT */
    sigaddset(&blocked_signals, SIGALRM);
    sigprocmask(SIGALRM, &blocked_signals, NULL);

    struct sigaction action;
    action.sa_handler = SIG_IGN;
    action.sa_flags = SA_SIGINFO;
    action.sa_sigaction = interrupt;
    if ((sigemptyset(&action.sa_mask) == -1) ||
        (sigaction(SIGALRM, &action, NULL) == -1)) {
        throw IOException(IOException::SIGNAL_HANDLER_INITIALIZE_ERROR);
    }
}

bool & Timer::time_out_occurred() {
    static bool time_out(false);
    return time_out;
}


/**---------------------------------------------------------------------------
 *- Helper Functions
 *---------------------------------------------------------------------------*/

bool is_file(const char * file_path) {
    return internal_is_file(file_path, true);
}

bool is_file_sans_logging(const char * file_path) {
    return internal_is_file(file_path, false);
}

// Throws exceptions if errors are detected.
size_t read_with_throw(int fd, char * const buf, size_t count) {
    ssize_t bytes_read = ::read(fd, buf, count);
    LogPtr log = Log::get_instance();
    checkGE0(log, bytes_read, IOException::READ_ERROR);
    return (size_t) bytes_read;
}

// Throws exceptions if errors are detected.
// Also unblocks time out signals before proceeding and throws
// ProcessTimeOutExceptions if they happen.
int select_with_throw(int nfds, fd_set * readfds, fd_set * writefds,
                      fd_set * errorfds, optional<double> seconds) {
    timespec time_out = timespec_from_seconds(!seconds ? 0.0 : seconds.get());
    sigset_t empty_set;
    sigemptyset(&empty_set);
    // Unblock all signals for the duration of select.
    int ready = -1;
    while(ready < 0)
    {
        ready = pselect(nfds, readfds, writefds, errorfds,
                            (!seconds ? NULL: &time_out), &empty_set);
        if (ready < 0) {
            if (errno == EINTR) {
                if (Timer::time_out_occurred()) {
                    throw TimeOutException();
                } else {
                    NOVA_LOG_ERROR("pselect was interrupted, restarting.");
                }
            } else {
                NOVA_LOG_ERROR2("Select returned < 0. errno = %d: %s\n "
                                "EINTR=%d", errno, strerror(errno), EINTR);
                throw IOException(IOException::GENERAL);
            }
        }
    }
    return ready;
}

} } }  // end nova::utils::io
