#include "nova/process.h"

#include "nova/Log.h"
#include <errno.h>
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
#include <sys/wait.h>

// Be careful with this Macro, as it comments out the entire line.
#ifdef _NOVA_PROCESS_VERBOSE
#define LOG_DEBUG(a) NOVA_LOG_DEBUG(a)
#define LOG_DEBUG2(a, b) NOVA_LOG_DEBUG2(a, b)
#define LOG_DEBUG3(a, b, c) NOVA_LOG_DEBUG2(a, b, c)
#define LOG_DEBUG8(a, b, c, d, e, f, g, h) NOVA_LOG_DEBUG2(a, b, c, d, e, f, g, h)
#else
#define LOG_DEBUG(a) /* log.debug(a) */
#define LOG_DEBUG2(a, b) /* log.debug(a, b) */
#define LOG_DEBUG3(a, b, c) /* log.debug(a, b, c) */
#define LOG_DEBUG8(a, b, c, d, e, f, g, h) /* log.debug(a, b, c, d, e, f, g, h)*/
#endif

extern char **environ;

using boost::optional;
using nova::ProcessException;
using nova::Log;
using namespace nova::utils;
using std::stringstream;
using std::string;
using nova::utils::io::TimeOutException;
using nova::utils::io::Timer;

bool time_out_occurred;

namespace nova {

namespace {

    inline void checkGE0(const int return_code,
                         ProcessException::Code code = ProcessException::GENERAL) {
        if (return_code < 0) {
            NOVA_LOG_ERROR2("System error : %s", strerror(errno));
            throw ProcessException(code);
        }
    }

    inline void checkEqual0(const int return_code,
                            ProcessException::Code code = ProcessException::GENERAL) {
        if (return_code != 0) {
            NOVA_LOG_ERROR2("System error : %s", strerror(errno));
            throw ProcessException(code);
        }
    }

    /** Translates a CommandList to the type requried by posix_spawn. */
    class ArgV : private boost::noncopyable
    {
        public:
            ArgV(const Process::CommandList & cmds)
            : new_argv(0), new_argv_length(0) {
                const size_t max = 2048;
                new_argv_length = cmds.size() + 1;
                new_argv = new char * [new_argv_length];
                size_t i = 0;
                BOOST_FOREACH(const char * cmd, cmds) {
                    new_argv[i ++] = strndup(cmd, max);
                }
                new_argv[new_argv_length - 1] = NULL;
            }

            ~ArgV() {
                for (size_t i = 0; i < new_argv_length; ++i) {
                    ::free(new_argv[i]);
                }
                delete[] new_argv;
                new_argv = 0;
                new_argv_length = 0;
            }

            char * * get() {
                return new_argv;
            }

        private:
            char * * new_argv;
            size_t new_argv_length;
    };

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

    void spawn_process(const Process::CommandList & cmds, pid_t * pid,
                       SpawnFileActions * actions=0)
    {
        if (cmds.size() < 1) {
            throw ProcessException(ProcessException::NO_PROGRAM_GIVEN);
        }
        const char * program_path = cmds.front();
        ArgV args(cmds);
        {
            stringstream str;
            str << "Running the following process: { ";
            BOOST_FOREACH(const char * cmd, cmds) {
                str << cmd;
                str << " ";
            }
            str << "}";
            LOG_DEBUG(str.str().c_str());
        }
        const posix_spawn_file_actions_t * file_actions = NULL;
        if (actions != 0) {
            file_actions = actions->get();
        }
        int status = posix_spawn(pid, program_path, file_actions, NULL,
                                 args.get(), environ);
        if (status != 0) {
            throw ProcessException(ProcessException::SPAWN_FAILURE);
        }
    }

}  // end anonymous namespace

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
 *- Process::Pipe
 *---------------------------------------------------------------------------*/

Process::Pipe::Pipe() {
    checkGE0(pipe(fd) < 0);
    is_open[0] = is_open[1] = true;
}

Process::Pipe::~Pipe() {
    close_in();
    close_out();
}

void Process::Pipe::close(int index) {
    if (is_open[index]) {
        checkEqual0(::close(fd[index]));
        is_open[index] = false;
    }
}

void Process::Pipe::close_in() {
    close(0);
}

void Process::Pipe::close_out() {
    close(1);
}


/**---------------------------------------------------------------------------
 *- Process
 *---------------------------------------------------------------------------*/

Process::Process(const CommandList & cmds, bool wait_for_close)
: argv(argv), eof_flag(false), std_in_pipe(), std_out_pipe(), success(false),
  wait_for_close(wait_for_close)
{
    SpawnFileActions file_actions;
    // Redirect process stdout / in to the pipes.
    file_actions.add_dup_to(std_in_pipe.in(), STDIN_FILENO);
    file_actions.add_dup_to(std_out_pipe.out(), STDOUT_FILENO);
    file_actions.add_dup_to(std_out_pipe.out(), STDERR_FILENO);
    file_actions.add_close(std_in_pipe.out());
    file_actions.add_close(std_out_pipe.in());

    spawn_process(cmds, &pid, &file_actions);

    // Close file descriptors on parent side.
    std_in_pipe.close_in();
    std_out_pipe.close_out();
}


Process::~Process() {
    set_eof();  // Close pipes.
}


void Process::create_argv(char * * & new_argv, int & new_argv_length,
                          const CommandList & cmds) {
    const size_t max = 2048;
    new_argv_length = cmds.size() + 1;
    new_argv = new char * [new_argv_length];
    size_t i = 0;
    BOOST_FOREACH(const char * cmd, cmds) {
        new_argv[i ++] = strndup(cmd, max);
    }
    new_argv[new_argv_length - 1] = NULL;
}

void Process::delete_argv(char * * & new_argv, int & new_argv_length) {
    for (int i = 0; i < new_argv_length; i ++) {
        ::free(new_argv[i]);
    }
    delete[] new_argv;
    new_argv = 0;
    new_argv_length = 0;
}

void Process::execute(const CommandList & cmds, double time_out) {
    stringstream str;
    execute(str, cmds, time_out);
}

void Process::execute(std::stringstream & out, const CommandList & cmds,
                      double time_out) {
    Process proc(cmds, true);
    proc.wait_for_eof(out, time_out);
    if (!proc.successful()) {
        throw ProcessException(ProcessException::EXIT_CODE_NOT_ZERO);
    }
}

pid_t Process::execute_and_abandon(const CommandList & cmds) {
    pid_t pid;
    spawn_process(cmds, &pid);
    return pid;
}

bool Process::is_pid_alive(pid_t pid) {
    // Send the "null signal," so kill only performs error checking but does not
    // actually send a signal.
    int result = ::kill(pid, 0);
    if (result == EINVAL && result == EPERM) {
        NOVA_LOG_ERROR2("Error calling kill with null signal: %s",
                        strerror(errno));
        throw ProcessException(ProcessException::GENERAL);
    }
    // ESRCH means o such process found.
    return result == 0;
}

size_t Process::read_into(stringstream & std_out, const optional<double> seconds) {
    LOG_DEBUG2("read_into with timeout=%f", !seconds ? 0.0 : seconds.get());
    if (eof_flag == true) {
        throw ProcessException(ProcessException::PROGRAM_FINISHED);
    }
    char buf[1048];
    if (!ready(std_out_pipe.in(), seconds)) {
        LOG_DEBUG("read_into: ready returned false, returning zero from read_into");
        return 0;
    }
    size_t count = io::read_with_throw(std_out_pipe.in(), buf, 1047);
    if (count == 0) {
        LOG_DEBUG("read returned 0, EOF");
        set_eof();
        return 0; // eof
    }
    LOG_DEBUG("Writing.");
    buf[count] = 0;  // Have to do this or Valgrind fails.
    std_out.write(buf, count);
    LOG_DEBUG3("count = %d, SO FAR %d", count, std_out.str().length());
    LOG_DEBUG2("OUTPUT:%s", buf);
    LOG_DEBUG("Exit read_into");
    return (size_t) count;
}

size_t Process::read_until_pause(stringstream & std_out,
                                 const double pause_time,
                                 const double time_out) {
    io::Timer timer(time_out);

    if (eof_flag == true) {
        throw ProcessException(ProcessException::PROGRAM_FINISHED);
    }
    size_t bytes_read = 0;
    size_t count;
    while(!eof() && (count = read_into(std_out, pause_time)) > 0) {
        bytes_read += count;
    }
    return bytes_read;
}

bool Process::ready(int file_desc, const optional<double> seconds) {
    if (eof_flag == true) {
        throw ProcessException(ProcessException::PROGRAM_FINISHED);
    }
    fd_set file_set;
    FD_ZERO(&file_set);
    FD_SET(file_desc, &file_set);
    return io::select_with_throw(file_desc + 1, &file_set, NULL, NULL, seconds)
           != 0;
}

void Process::set_eof() {
    if (!eof()) {
        eof_flag = true;
        std_out_pipe.close_in();
        std_in_pipe.close_out();
        int status;
        int child_pid;
        int options = wait_for_close ? 0 : WNOHANG;
        while(((child_pid = waitpid(pid, &status, options)) == -1)
              && (errno == EINTR));
        #ifdef _NOVA_PROCESS_VERBOSE
            NOVA_LOG_DEBUG2("Child exited. child_pid=%d, pid=%d, Pid==pid=%s, "
                            "WIFEXITED=%d, WEXITSTATUS=%d, "
                            "WIFSIGNALED=%d, WIFSTOPPED=%d",
                            child_pid, pid,
                            (child_pid == pid ? "true" : "false"),
                            WIFEXITED(status), (int) WEXITSTATUS(status),
                            WIFSIGNALED(status), WIFSTOPPED(status));
        #endif
        success = child_pid == pid && WIFEXITED(status)
                  && WEXITSTATUS(status) == 0;
    }
}

void Process::wait_for_eof(double seconds) {
    stringstream str;
    wait_for_eof(str, seconds);
}

void Process::wait_for_eof(stringstream & out, double seconds) {
    LOG_DEBUG2("wait_for_eof, timeout=%f", seconds);
    Timer timer(seconds);
    while(read_into(out, optional<double>(seconds)));
    if (!eof()) {
        NOVA_LOG_ERROR2("Something went wrong, EOF not reached! Time out=%f",
                        seconds);
        throw TimeOutException();
    }
}

void Process::write(const char * msg) {
    const size_t maxlen = 2048;
    size_t length = (size_t) strnlen(msg, maxlen);
    if (length == maxlen) {
        NOVA_LOG_ERROR("String was not null terminated.");
        throw ProcessException(ProcessException::GENERAL);
    }
    this->write(msg, length);
}

void Process::write(const char * msg, size_t length) {
    //::write(std_in_fd[0], msg, length);
    LOG_DEBUG2("Writing msg with %d bytes.", length);
    ssize_t count = ::write(std_in_pipe.out(), msg, length);
    if (count < 0) {
        NOVA_LOG_ERROR2("write failed. errno = %d", errno);
        throw ProcessException(ProcessException::GENERAL);
    } else if (count != ((ssize_t)length)) {
        NOVA_LOG_ERROR2("Did not write our message! errno = %d", errno);
        throw ProcessException(ProcessException::GENERAL);
    }
}

}  // end nova namespace
