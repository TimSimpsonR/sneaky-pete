#include "nova/process.h"

#include "nova/log.h"
#include <errno.h>
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

// Be careful with this Macro, as it comments out the entire line.
#ifdef _NOVA_PROCESS_VERBOSE
#define LOG_DEBUG log.debug
#else
#define LOG_DEBUG //
#endif

extern char **environ;

using boost::optional;
using nova::ProcessException;
using nova::Log;
using namespace nova::utils;
using std::stringstream;
using std::string;
using nova::utils::io::Timer;

bool time_out_occurred;

namespace {

    inline void checkGE0(Log & log, const int return_code,
                         ProcessException::Code code = ProcessException::GENERAL) {
        if (return_code < 0) {
            log.error2("System error : %s", strerror(errno));
            throw ProcessException(code);
        }
    }

    inline void checkEqual0(Log & log, const int return_code,
                            ProcessException::Code code = ProcessException::GENERAL) {
        if (return_code != 0) {
            log.error2("System error : %s", strerror(errno));
            throw ProcessException(code);
        }
    }

}  // end anonymous namespace

namespace nova {


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
        case PROGRAM_FINISHED:
            return "Program is already finished.";
        default:
            return "An error occurred.";
    }
}


/**---------------------------------------------------------------------------
 *- Process
 *---------------------------------------------------------------------------*/

Process::Process(const char * program_path, const char * const argv[])
: argv(argv), eof_flag(false), log(), program_path(program_path)
{
     // Remember 0 is for reading, 1 is for writing.
    checkGE0(log, pipe(std_out_fd) < 0);
    checkGE0(log, pipe(std_in_fd) < 0);

    posix_spawn_file_actions_t file_actions;
    checkEqual0(log, posix_spawn_file_actions_init(&file_actions));
    // Redirect process stdout / in to the pipes.
    checkEqual0(log, posix_spawn_file_actions_adddup2(&file_actions,
        std_in_fd[0], STDIN_FILENO));
    checkEqual0(log, posix_spawn_file_actions_adddup2(&file_actions,
        std_out_fd[1], STDOUT_FILENO));
    checkEqual0(log, posix_spawn_file_actions_adddup2(&file_actions,
        std_out_fd[1], STDERR_FILENO));
    checkEqual0(log, posix_spawn_file_actions_addclose(&file_actions,
        std_in_fd[1]));
    checkEqual0(log, posix_spawn_file_actions_addclose(&file_actions,
        std_out_fd[0]));

    char * * new_argv;
    int new_argv_length;
    create_argv(new_argv, new_argv_length, program_path, argv);
    pid_t pid;
    int status = posix_spawn(&pid, program_path, &file_actions, NULL,
                             new_argv, environ);
    delete_argv(new_argv, new_argv_length);
    posix_spawn_file_actions_destroy(&file_actions);

    // Close file descriptors on parent side.
    checkEqual0(log, close(std_in_fd[0]));
    checkEqual0(log, close(std_out_fd[1]));

    if (status != 0) {
        throw ProcessException(ProcessException::SPAWN_FAILURE);
    }
}


Process::~Process() {
    close(std_out_fd[0]);
    close(std_in_fd[0]);
}

void Process::create_argv(char * * & new_argv, int & new_argv_length,
                          const char * program_path,
                          const char * const argv[]) {
    int old_argv_length = 0;
    while(argv[old_argv_length] != 0) {
      old_argv_length ++;
    }
    const size_t max = 2048;
    new_argv_length = old_argv_length + 2;
    new_argv = new char * [new_argv_length];
    new_argv[0] = strndup(program_path, max);
    for (int i = 0; i < old_argv_length; i ++) {
      new_argv[i + 1] = strndup(argv[i], max);
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


size_t Process::read_into(stringstream & std_out, const optional<double> seconds) {
    if (eof_flag == true) {
        throw ProcessException(ProcessException::PROGRAM_FINISHED);
    }
    char buf[1048];
    if (!ready(std_out_fd[0], seconds)) {
        return 0;
    }
    size_t count = io::read_with_throw(log, std_out_fd[0], buf, 1047);
    if (count == 0) {
        eof_flag = true;
        LOG_DEBUG("read returned 0, EOF");
        return 0; // eof
    }
    LOG_DEBUG("Writing.");
    std_out.write(buf, count);
    LOG_DEBUG("count = %d, SO FAR %d", count, std_out.str().length());
    buf[count] = 0;  // Have to do this or Valgrind fails.
    LOG_DEBUG("OUTPUT:%s", buf);
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
    fd_set file_set;
    FD_ZERO(&file_set);
    FD_SET(file_desc, &file_set);
    return io::select_with_throw(file_desc + 1, &file_set, NULL, NULL, seconds);
}

void Process::wait_for_eof(double seconds) {
    stringstream str;
    Timer timer(seconds);
    while(read_into(str, optional<double>(seconds) > 0));
    if (!eof()) {
        log.error("Something went wrong, EOF not reached!");
    }
}

void Process::write(const char * msg) {
    const size_t maxlen = 2048;
    size_t length = (size_t) strnlen(msg, maxlen);
    if (length == maxlen) {
        log.error2("String was not null terminated.");
        throw ProcessException(ProcessException::GENERAL);
    }
    this->write(msg, length);
}

void Process::write(const char * msg, size_t length) {
    //::write(std_in_fd[0], msg, length);
    LOG_DEBUG("Writing msg with %d bytes.", length);
    ssize_t count = ::write(std_in_fd[1], msg, length);
    if (count < 0) {
        log.error2("write failed. errno = %d", errno);
        throw ProcessException(ProcessException::GENERAL);
    } else if (count != ((ssize_t)length)) {
        log.error2("Did not write our message! errno = %d", errno);
        throw ProcessException(ProcessException::GENERAL);
    }
}

}  // end nova namespace
