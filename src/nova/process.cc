#include "pch.hpp"
#include "nova/process.h"

#include "nova/Log.h"
#include <errno.h>
#include <fcntl.h> // Consider moving to io.cc and using there.
#include <boost/foreach.hpp>
#include <fstream>
#include "nova/utils/io.h"
#include <iostream>
#include <malloc.h>  // Valgrind complains if we don't use "free" below. ;_;
#include <signal.h>
#include <sys/select.h>
#include <spawn.h>
#include <sstream>
#include <stdio.h>
#include <stdlib.h> // exit
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>

extern char **environ;

using boost::optional;
using nova::Log;
using nova::utils::io::Pipe;
using namespace nova::utils;
using std::stringstream;
using std::string;
using nova::utils::io::TimeOutException;
using nova::utils::io::Timer;
using nova::utils::io::wait_pid_with_throw;

bool time_out_occurred;

namespace nova { namespace process {

namespace {

    inline void checkEqual0(const int return_code,
                            ProcessException::Code code = ProcessException::GENERAL) {
        if (return_code != 0) {
            NOVA_LOG_ERROR("System error : %s", strerror(errno));
            throw ProcessException(code);
        }
    }

    void kill_with_throw(pid_t pid, int sig, bool allow_not_found=false) {
        if (0 != ::kill(pid, sig)) {
            NOVA_LOG_ERROR("Couldn't send kill signal!! %s", strerror(errno));
            if (allow_not_found && ESRCH == errno){
                NOVA_LOG_INFO("errno was ESRCH and that's OK.");
                return;
            }
            throw ProcessException(ProcessException::KILL_SIGNAL_ERROR);
        }
    }


    /** Translates a CommandList to the type requried by posix_spawn. */
    class ArgV : private boost::noncopyable
    {
        public:
            ArgV(const CommandList & cmds)
            :   new_argv_length(cmds.size() + 1),
                new_argv(new char * [new_argv_length])
            {
                new_argv = new char * [new_argv_length];
                size_t i = 0;
                BOOST_FOREACH(const string & cmd, cmds) {
                    new_argv[i ++] = strndup(cmd.c_str(), cmd.size());
                }
                new_argv[new_argv_length - 1] = NULL;
            }

            ~ArgV() {
                for (size_t i = 0; i < new_argv_length; ++i) {
                    ::free(new_argv[i]);
                }
                delete[] new_argv;
                new_argv = 0;
            }

            char * * get() {
                return new_argv;
            }

        private:
            const size_t new_argv_length;
            char * * new_argv;
    };

    const size_t BUFFER_SIZE = 1048;

    /** Waits for the given file descriptor to have more data for the given
     *  number of seconds. */
    bool ready(int file_desc, const optional<double> seconds) {
        fd_set file_set;
        FD_ZERO(&file_set);
        FD_SET(file_desc, &file_set);
        return io::select_with_throw(file_desc + 1, &file_set, NULL, NULL, seconds)
               != 0;
    }

    /* If neither file descriptor has input by the time denoted by "seconds",
     * boost::none is returned. Otherwise, the filedesc which was ready is
     * returned. */
    optional<int> ready(int file_desc1, int file_desc2,
                        const optional<double> seconds) {
        fd_set file_set;
        FD_ZERO(&file_set);
        FD_SET(file_desc1, &file_set);
        FD_SET(file_desc2, &file_set);
        int nfds = std::max(file_desc1, file_desc2) + 1;
        auto result = io::select_with_throw(nfds, &file_set, NULL,
                                            NULL, seconds);
        if (0 == result) {
            return boost::none;
        } else {
            if (FD_ISSET(file_desc1, &file_set)) {
                return file_desc1;
            } else {
                return file_desc2;
            }
        }
    }

}  // end anonymous namespace


/**---------------------------------------------------------------------------
 *- SpawnFileActions
 *---------------------------------------------------------------------------*/

/**
 *  Wraps posix_spawn_file_actions_t to make sure the destroy method is
 *  called.
 */
class SpawnFileActions : private boost::noncopyable
{
    public:
        SpawnFileActions() {
            checkEqual0(posix_spawn_file_actions_init(&file_actions));
        }

        ~SpawnFileActions() {
            posix_spawn_file_actions_destroy(&file_actions);
        }

        void add_close(int fd) {
            checkEqual0(posix_spawn_file_actions_addclose(&file_actions, fd));
        }

        void add_dup_to(int fd, int new_fd) {
            checkEqual0(posix_spawn_file_actions_adddup2(&file_actions,
                fd, new_fd));
        }

        inline posix_spawn_file_actions_t * get() {
            return &file_actions;
        }

    private:
        posix_spawn_file_actions_t file_actions;
};

namespace {
    /* This function really does everything related to "running a process",
     * all the rest of this junk is just for monitoring that process.
     * See "execute_and_abandon" for the simplest possible way to use this. */
    void spawn_process(const CommandList & cmds, pid_t * pid,
                       SpawnFileActions * actions=0)
    {
        if (cmds.size() < 1) {
            throw ProcessException(ProcessException::NO_PROGRAM_GIVEN);
        }
        const string program_path = cmds.front();
        ArgV args(cmds);
        {
            stringstream str;
            str << "Running the following process: { ";
            BOOST_FOREACH(const string & cmd, cmds) {
                str << "'" << cmd << "'";
                str << " ";
            }
            str << "}";
            NOVA_LOG_DEBUG(str.str().c_str());
        }
        const posix_spawn_file_actions_t * file_actions = NULL;
        if (actions != 0) {
            file_actions = actions->get();
        }
        int status = posix_spawn(pid, program_path.c_str(), file_actions, NULL,
                                 args.get(), environ);
        if (status != 0) {
            throw ProcessException(ProcessException::SPAWN_FAILURE);
        }
    }
} // end second anonymous namespace


/**---------------------------------------------------------------------------
 *- Global Functions
 *---------------------------------------------------------------------------*/

void execute(const CommandList & cmds, optional<double> time_out) {
    Process<> proc(cmds);
    if (time_out) {
        proc.wait_for_exit(time_out.get());
    } else {
        NOVA_LOG_INFO("Warning: Waiting forever for process to end.");
        proc.wait_forever_for_exit();
    }
    if (!proc.successful()) {
        throw ProcessException(ProcessException::EXIT_CODE_NOT_ZERO);
    }
}

void execute_with_stdout_and_stderr(const CommandList & cmds, double time_out, bool check_proc) {
    Process<StdErrAndStdOut> proc(cmds);
    proc.wait_for_exit(time_out);
    if (check_proc && !proc.successful()) {
        throw ProcessException(ProcessException::EXIT_CODE_NOT_ZERO);
    }
}

void execute(std::stringstream & out, const CommandList & cmds,
             double time_out) {
    Process<StdErrAndStdOut> proc(cmds); //, true);
    try {
        proc.read_into_until_exit(out, time_out);
    } catch(const TimeOutException & toe) {
        NOVA_LOG_ERROR("Timeout error occurred reading until eof.");
        // This is what the code used to do, but maybe it would be good to
        // double check that this is desired.
        proc.wait_forever_for_exit();
        throw;
    }
    proc.wait_forever_for_exit();
    if (!proc.successful()) {
        throw ProcessException(ProcessException::EXIT_CODE_NOT_ZERO);
    }
}

pid_t execute_and_abandon(const CommandList & cmds) {
    pid_t pid;
    spawn_process(cmds, &pid);
    return pid;
}

bool is_pid_alive(pid_t pid) {
    // Send the "null signal," so kill only performs error checking but does not
    // actually send a signal.
    int result = ::kill(pid, 0);
    if (result == EINVAL && result == EPERM) {
        NOVA_LOG_ERROR("Error calling kill with null signal: %s",
                       strerror(errno));
        throw ProcessException(ProcessException::GENERAL);
    }
    // ESRCH means o such process found.
    return result == 0;
}


/**---------------------------------------------------------------------------
 *- ProcessException
 *---------------------------------------------------------------------------*/

ProcessException::ProcessException(Code code) throw()
: code(code) {
}

ProcessException::~ProcessException() throw() {
}

const char * ProcessException::what() const throw() {
    switch(code) {
        case EXIT_CODE_NOT_ZERO:
            return "The exit code was not zero.";
        case NO_PROGRAM_GIVEN:
            return "No program to launch was given (first element was null).";
        case PROGRAM_FINISHED:
            return "Program is already finished.";
        default:
            return "An error occurred.";
    }
}


/**---------------------------------------------------------------------------
 *- ProcessStatusWatcher
 *---------------------------------------------------------------------------*/

ProcessStatusWatcher::ProcessStatusWatcher()
: finished_flag(false), success(false)
{


}

ProcessStatusWatcher::~ProcessStatusWatcher() {
    wait_for_exit_code(false);  // Close pipes.
}

int ProcessStatusWatcher::call_waitpid(int * status, bool do_not_wait) {
    const int options = do_not_wait ? WNOHANG : 0 ;
    return wait_pid_with_throw(pid, status, options);
}

void ProcessStatusWatcher::wait_for_exit_code(bool wait_forever) {
    if (!is_finished()) {
        finished_flag = true;
        int status;
        int child_pid;
        if (!wait_forever) {
            // Here's the thing- WNOHANG causes waitpid to return 0 (fail)
            // nearly every time. So it's better to not specify it, even if
            // the calling code wanted it, and use a Timer to bust out if
            // we risk hanging forever. Then we can use WNOHANG as a last
            // resort.
            try {
                Timer timer(1);
                child_pid = call_waitpid(&status);
            } catch(const TimeOutException & toe) {
                NOVA_LOG_ERROR("Timed out calling waitpid without WNOHANG.");
                child_pid = call_waitpid(&status, true);
            }
        } else {
            child_pid = call_waitpid(&status);
        }
        #ifdef _NOVA_PROCESS_VERBOSE
            NOVA_LOG_TRACE("Child exited. wait_forever=%s child_pid=%d, "
                           "pid=%d, Pid==pid=%s, "
                           "WIFEXITED=%d, WEXITSTATUS=%d, "
                           "WIFSIGNALED=%d, WIFSTOPPED=%d",
                           (wait_forever ? "true" : "false"),
                           child_pid, pid,
                           (child_pid == pid ? "true" : "false"),
                           WIFEXITED(status), (int) WEXITSTATUS(status),
                           WIFSIGNALED(status), WIFSTOPPED(status));
        #endif
        success = child_pid == pid && WIFEXITED(status)
                  && WEXITSTATUS(status) == 0;
    }
}


/**---------------------------------------------------------------------------
 *- ProcessBase
 *---------------------------------------------------------------------------*/

ProcessBase::ProcessBase()
:   io_watchers(),
    status_watcher()
{
}

ProcessBase::~ProcessBase() {
}


void ProcessBase::add_io_handler(ProcessFileHandler * handler) {
    io_watchers.push_back(handler);
}

void ProcessBase::destroy() {
    _wait_for_exit_code(false); // Close pipes.
}

void ProcessBase::drain_io_from_file_handlers(optional<double> seconds) {
    BOOST_FOREACH(ProcessFileHandler * ptr, io_watchers) {
        ptr->drain_io(seconds);
    }
}

void ProcessBase::initialize(const CommandList & cmds) { //, ProcessIOList io_list) {
    SpawnFileActions file_actions;
    pre_spawn_stderr_actions(file_actions);
    pre_spawn_stdin_actions(file_actions);
    pre_spawn_stdout_actions(file_actions);

    spawn_process(cmds, &(status_watcher.get_pid()), &file_actions);

    BOOST_FOREACH(ProcessFileHandler * const ptr, io_watchers) {
        ptr->post_spawn_actions();
    }
}

void ProcessBase::kill(double initial_wait_time,
                       optional<double> serious_wait_time) {
    kill_with_throw(status_watcher.get_pid(), SIGTERM);
    try {
        wait_for_exit(5);
    } catch (const TimeOutException & toe) {
        if (!serious_wait_time) {
            throw;
        } else {
            NOVA_LOG_ERROR("Won't die, eh? Then its time to use our ultimate "
                           "weapon.");
            kill_with_throw(status_watcher.get_pid(), SIGKILL, true);
            wait_for_exit(15);
        }
    }
}

void ProcessBase::pre_spawn_stderr_actions(SpawnFileActions & file_actions) {
    file_actions.add_close(STDERR_FILENO);
}

void ProcessBase::pre_spawn_stdin_actions(SpawnFileActions & file_actions) {
    file_actions.add_close(STDIN_FILENO);
}

void ProcessBase::pre_spawn_stdout_actions(SpawnFileActions & file_actions) {
    file_actions.add_close(STDOUT_FILENO);
}

void ProcessBase::_wait_for_exit_code(bool wait_forever) {
    if (!status_watcher.is_finished()) {
        BOOST_FOREACH(ProcessFileHandler * const ptr, io_watchers) {
            ptr->set_eof_actions();
        }
        status_watcher.wait_for_exit_code(wait_forever);
    }
}

void ProcessBase::wait_for_exit(double seconds) {
    NOVA_LOG_DEBUG("Waiting for %f seconds for EOF...", seconds);
    drain_io_from_file_handlers(seconds);
    Timer timer(seconds);
    wait_forever_for_exit();
}

void ProcessBase::wait_forever_for_exit() {
    NOVA_LOG_DEBUG("Waiting forever for EOF...");
    drain_io_from_file_handlers(boost::none);
    _wait_for_exit_code(true);
}


/**---------------------------------------------------------------------------
 *- IndependentStdErrAndStdOut
 *---------------------------------------------------------------------------*/

IndependentStdErrAndStdOut::IndependentStdErrAndStdOut()
:   std_err_pipe(),
    std_out_pipe()
{
    ::fcntl(std_out_pipe.in(), F_SETFL, O_NONBLOCK);
    ::fcntl(std_err_pipe.in(), F_SETFL, O_NONBLOCK);
    add_io_handler(this);
}

IndependentStdErrAndStdOut::~IndependentStdErrAndStdOut() {
}

void IndependentStdErrAndStdOut::drain_io(optional<double> seconds) {
    if (draining) {
        return;
    }
    NOVA_LOG_TRACE("Draining STDOUT / STDERR...");
    draining = true;
    char buffer[1024];
    std::unique_ptr<Timer> timer;
    if (seconds) {
        timer.reset(new Timer(seconds.get()));
    }
    ReadResult result;
    while((result = read_into(buffer, sizeof(buffer), seconds)).write_length
          != 0) {
        NOVA_LOG_TRACE("Draining again! %d", result.write_length);
    }
}

void IndependentStdErrAndStdOut::post_spawn_actions() {
    std_out_pipe.close_out();
    std_err_pipe.close_out();
    NOVA_LOG_TRACE("Closing the out side of the stderr/stdout pipe.");
}

void IndependentStdErrAndStdOut::pre_spawn_stderr_actions(
    SpawnFileActions & file_actions)
{
    NOVA_LOG_TRACE("Opening up stderr pipes.");
    file_actions.add_dup_to(std_err_pipe.out(), STDERR_FILENO);
    file_actions.add_close(std_err_pipe.in());
}

void IndependentStdErrAndStdOut::pre_spawn_stdout_actions(
    SpawnFileActions & file_actions)
{
    NOVA_LOG_TRACE("Opening up stdout pipes.");
    file_actions.add_dup_to(std_out_pipe.out(), STDOUT_FILENO);
    file_actions.add_close(std_out_pipe.in());
}

void IndependentStdErrAndStdOut::set_eof_actions() {
    std_out_pipe.close_in();
    std_err_pipe.close_in();
    NOVA_LOG_TRACE("Closing in side of the stderr and stdout pipes.");
}

IndependentStdErrAndStdOut::ReadResult IndependentStdErrAndStdOut::read_into(
    char * buffer, const size_t length, const optional<double> seconds)
{
    NOVA_LOG_TRACE("read_into with timeout=%f", !seconds ? 0.0 : seconds.get());
    if (!std_out_pipe.in_is_open()) {
        if (!std_err_pipe.in_is_open()) {
            throw ProcessException(ProcessException::PROGRAM_FINISHED);
        } else {
            return _read_into(buffer, length, seconds);
        }
    }
    const auto result = ready(this->std_out_pipe.in(), this->std_err_pipe.in(),
                              seconds);
    if (!result) {
        NOVA_LOG_TRACE("ready returned nothing. Returning NA from read_into");
        return { ReadResult::NA, 0 };
    }
    const int file_desc = result.get();
    const size_t count = io::read_with_throw(file_desc, buffer, length);
    if (count == 0) {
        // If read returns zero, it means the filedesc is EOF. Set whichever
        // one it was to closed and then use the other _read_into method.
        if (file_desc == std_out_pipe.in()) {
            std_out_pipe.close_in();
        } else {
            std_err_pipe.close_in();
        }
        return _read_into(buffer, length, seconds);
    }
    // Data was read, so return info on which stream it came from.
    return {
        (file_desc == std_out_pipe.in() ? ReadResult::StdOut
                                        : ReadResult::StdErr),
        count
    };
}

IndependentStdErrAndStdOut::ReadResult IndependentStdErrAndStdOut::_read_into(
    char * buffer, const size_t length, const optional<double> seconds)
{
    int filedesc;
    ReadResult::FileIndex index;
    if (std_out_pipe.in_is_open()) {
        filedesc = std_out_pipe.in();
        index = ReadResult::FileIndex::StdOut;
    } else {
        filedesc = std_err_pipe.in();
        index = ReadResult::FileIndex::StdErr;
    }
    NOVA_LOG_TRACE("read_into with timeout=%f", !seconds ? 0.0 : seconds.get());
    if (!ready(filedesc, seconds)) {
        NOVA_LOG_TRACE("ready returned false, returning zero from read_into");
        return { ReadResult::NA, 0 };
    }
    size_t count = io::read_with_throw(filedesc, buffer, length);
    if (count == 0) {
        NOVA_LOG_TRACE("read returned 0, EOF");
        draining = true;  // Avoid re-draining.
        wait_forever_for_exit();
        return { ReadResult::NA, 0 };
    }
    return { index, count };
}

/**---------------------------------------------------------------------------
 *- StdIn
 *---------------------------------------------------------------------------*/

StdIn::StdIn()
:   std_in_pipe()
{
    add_io_handler(this);
}

StdIn::~StdIn() {
}

void StdIn::set_eof_actions() {
    std_in_pipe.close_out();
}

void StdIn::pre_spawn_stdin_actions(SpawnFileActions & file_actions) {
    file_actions.add_dup_to(std_in_pipe.in(), STDIN_FILENO);
    file_actions.add_close(std_in_pipe.out());
}

void StdIn::post_spawn_actions() {
    std_in_pipe.close_in();
}

void StdIn::write(const char * msg) {
    const size_t maxlen = 2048;
    size_t length = (size_t) strnlen(msg, maxlen);
    if (length == maxlen) {
        NOVA_LOG_ERROR("String was not null terminated.");
        throw ProcessException(ProcessException::GENERAL);
    }
    this->write(msg, length);
}

void StdIn::write(const char * msg, size_t length) {
    //::write(std_in_fd[0], msg, length);
    NOVA_LOG_TRACE("Writing msg with %d bytes.", length);
    ssize_t count = ::write(this->std_in_pipe.out(), msg, length);
    if (count < 0) {
        NOVA_LOG_ERROR("write failed. errno = %d", errno);
        throw ProcessException(ProcessException::GENERAL);
    } else if (count != ((ssize_t)length)) {
        NOVA_LOG_ERROR("Did not write our message! errno = %d", errno);
        throw ProcessException(ProcessException::GENERAL);
    }
}


/**---------------------------------------------------------------------------
 *- StdErr
 *---------------------------------------------------------------------------*/

StdErrToFile::StdErrToFile() {
}

StdErrToFile::~StdErrToFile() {
}

void StdErrToFile::post_spawn_actions() {
    ::close(file_descriptor);
}

void StdErrToFile::pre_spawn_stderr_actions(SpawnFileActions & file_actions) {
    file_descriptor = open(log_file_name(), O_WRONLY | O_APPEND | O_CREAT);
    file_actions.add_dup_to(file_descriptor, STDERR_FILENO);
}


/**---------------------------------------------------------------------------
 *- StdErrAndStdOut
 *---------------------------------------------------------------------------*/

StdErrAndStdOut::StdErrAndStdOut()
:   draining(false),
    std_out_pipe()
{
    ::fcntl(std_out_pipe.in(), F_SETFL, O_NONBLOCK);
    add_io_handler(this);
}

StdErrAndStdOut::~StdErrAndStdOut()
{
}

void StdErrAndStdOut::drain_io(optional<double> seconds) {
    if (draining) {
        return;
    }
    NOVA_LOG_TRACE("Draining STDOUT / STDERR...");
    draining = true;
    char buffer[1024];
    size_t count;
    std::auto_ptr<Timer> timer;
    if (seconds) {
        timer.reset(new Timer(seconds.get()));
    }
    while (0 != (count = read_into(buffer, sizeof(buffer) - 1, seconds))) {
        NOVA_LOG_TRACE("Draining again! %d", count);
    };
}

void StdErrAndStdOut::pre_spawn_stderr_actions(SpawnFileActions & file_actions) {
    NOVA_LOG_TRACE("Opening up stderr pipes.");
    file_actions.add_dup_to(std_out_pipe.out(), STDERR_FILENO);
}

void StdErrAndStdOut::pre_spawn_stdout_actions(SpawnFileActions & file_actions) {
    NOVA_LOG_TRACE("Opening up stdout pipes.");
    file_actions.add_dup_to(std_out_pipe.out(), STDOUT_FILENO);
    file_actions.add_close(std_out_pipe.in());
}

void StdErrAndStdOut::post_spawn_actions() {
    std_out_pipe.close_out();
    NOVA_LOG_TRACE("Closing the out side of the stderr/stdout pipe.");
}

size_t StdErrAndStdOut::read_into(stringstream & std_out,
                                  const optional<double> seconds) {
    char buf[BUFFER_SIZE];
    for (size_t i = 0; i < BUFFER_SIZE; ++i)
    {
        buf[i] = '~';
    }
    size_t count = read_into(buf, BUFFER_SIZE-1, seconds);
    buf[count] = 0;  // Have to do this or Valgrind fails.
    std_out.write(buf, count);
    NOVA_LOG_TRACE("buffer output:%s", buf);
    NOVA_LOG_TRACE("count = %d, SO FAR %d", count, std_out.str().length());
    return count;
}

size_t StdErrAndStdOut::read_into(char * buffer, const size_t length,
                                  const optional<double> seconds) {
    NOVA_LOG_TRACE("read_into with timeout=%f", !seconds ? 0.0 : seconds.get());
    if (!std_out_pipe.in_is_open()) {
        throw ProcessException(ProcessException::PROGRAM_FINISHED);
    }
    if (!ready(this->std_out_pipe.in(), seconds)) {
        NOVA_LOG_TRACE("ready returned false, returning zero from read_into");
        return 0;
    }
    size_t count = io::read_with_throw(this->std_out_pipe.in(), buffer, length);
    if (count == 0) {
        NOVA_LOG_TRACE("read returned 0, EOF");
        draining = true;  // Avoid re-draining.
        wait_forever_for_exit();
        return 0; // eof
    }
    return (size_t) count;
}

void StdErrAndStdOut::read_into_until_exit(stringstream & out,
                                           double seconds) {
    NOVA_LOG_TRACE("wait_for_eof, timeout=%f", seconds);
    while(read_into(out, optional<double>(seconds)));
    if (std_out_pipe.in_is_open()) {
        NOVA_LOG_ERROR("Something went wrong, EOF not reached! Time out=%f",
                       seconds);
        throw TimeOutException();
    }
}

size_t StdErrAndStdOut::read_until_pause(stringstream & std_out,
                                         const double time_out) {
    if (!std_out_pipe.in_is_open()) {
        throw ProcessException(ProcessException::PROGRAM_FINISHED);
    }
    size_t bytes_read = 0;
    size_t count;
    while(std_out_pipe.in_is_open() && (count = read_into(std_out, time_out)) > 0) {
        bytes_read += count;
    }
    return bytes_read;
}

void StdErrAndStdOut::set_eof_actions() {
    std_out_pipe.close_in();
    NOVA_LOG_TRACE("Closing in side of the stderr/stdout pipe.");
}


} }  // end nova::process namespace
