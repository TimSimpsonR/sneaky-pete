#ifndef __NOVA_PROCESS_H
#define __NOVA_PROCESS_H


#include "nova/utils/io.h"
#include <boost/optional.hpp>
#include "nova/Log.h"
#include <list>
#include <sstream>
#include <boost/utility.hpp>
#include <vector>

/**
 *  This namespace allows you to create and manipulate processes using the
 *  Process type.
 *
 *  Processes start when the constructor runs and continue until "wait_for_exit"
 *  or "wait_forever_for_exit" is called. At this point (and no earlier)
 *  "is_finished()" will return true and "successful()" will return true if
 *  the process ended normally and the exit code was zero.
 *
 *  The Process class is actually a template which can take an empty type
 *  parameter list. If you need to read or write to a process's standard
 *  streams, the following classes can be added to the template argument list
 *  (they can be thought of as mix-ins):
 *
 *  StdIn - This adds methods to read from STDIN.
 *
 *  StdErrAndStdOut - This treats a process's STDERR and STDOUT stream as a
 *  single, unified stream which can be read from.
 *
 *  This example calls ls and reads the current directories. If ls takes
 *  longer than 60 seconds to time out, it throws a TimeOutException:
 *
 *  std::stringstream buffer;
 *  CommandList cmds = list_of("ls");
 *  Process<StdErrAndStdOut> proc(cmds);
 *  proc.read_into_until_exit(buffer, 60);
 *  std::vector<std::string> files;
 *  while (buffer.good()) {
 *     std::string file << buffer;
 *     files.push_back();
 *  }
 */
namespace nova { namespace process {


/** Simple list of commands. */
typedef std::list<std::string> CommandList;

/** Executes the given command, waiting until its finished. Returns
 *  true if the command runs successfully with a good exit code, false
 *  otherwise. */
void execute(const CommandList & cmds, double time_out=30);

/** Like the corresponding "execute" command but pipes stdout / stderr to
 *  a stream. Some processes need this to function correctly! */
void execute_with_stdout_and_stderr(const CommandList & cmds,
                                    double time_out=30, bool check_proc=true);

/** Similar to execute, but throws a TimeOutException if any reads take
 *  longer than the time_out argument. */
void execute(std::stringstream & out, const CommandList & cmds,
             double time_out=30);

/** Executes the given command but does not wait until its finished.
 *  This function is an anomaly because unlike most functions of this
 *  class it does not open up the process's streams or wait for it. */
pid_t execute_and_abandon(const CommandList & cmds);

/** Returns true if the given pid is alive. */
bool is_pid_alive(pid_t pid);


class ProcessException : public std::exception {

    public:
        enum Code {
            EXIT_CODE_NOT_ZERO,
            GENERAL,
            KILL_SIGNAL_ERROR,
            NO_PROGRAM_GIVEN,
            PROGRAM_FINISHED,
            SPAWN_FAILURE
        };

        ProcessException(Code code) throw();

        virtual ~ProcessException() throw();

        const Code code;

        virtual const char * what() const throw();
};


class SpawnFileActions;


/** Holds onto a pid ID and monitors it, grabbing the exit code when it dies. */
class ProcessStatusWatcher : private boost::noncopyable {
    public:
        /** Creates an object which will get the status of pid. If
         *  "wait_for_close" is true, the program will hang until the process
         *  identified by pid dies. */
        ProcessStatusWatcher();

        ~ProcessStatusWatcher();

        inline pid_t & get_pid() {
            return pid;
        }

        inline const pid_t & get_pid() const {
            return pid;
        }

        /** True if the process has ended. */
        inline bool is_finished() const {
            return finished_flag;
        }

        /** This is always false until eof() returns true. */
        inline bool successful() const {
            return success;
        }

        /** Called when you're ready to get the processes exit status.
         *  This needs to be when you're finished with the process and
         *  perceive it to have completed.*/
        void wait_for_exit_code(bool wait_forever);

    private:
        bool finished_flag;
        pid_t pid;
        bool success;

        int call_waitpid(int * status, bool do_not_wait=false);
};


class ProcessFileHandler;

class ProcessBase : private boost::noncopyable {

    public:
        ~ProcessBase();

        inline bool is_finished() const {
            return status_watcher.is_finished();
        }

        inline pid_t & get_pid() {
            return status_watcher.get_pid();
        }

        void kill(double initial_wait_time,
                  boost::optional<double> serious_wait_time);

        inline bool successful() const {
            return status_watcher.successful();
        }

        /** Waits for the process to exit so it can retrieve it's exit code.
         *  Throws TimeOutException if it doesn't happen. */
        void wait_for_exit(double seconds);

        /** Waits for the process to exit. Does not time out.
         *  Does not collect stdout. */
        void wait_forever_for_exit();

    protected:
        ProcessBase();

        void add_io_handler(ProcessFileHandler * handler);

        void destroy();

        void drain_io_from_file_handlers(boost::optional<double> seconds);

        void initialize(const CommandList & cmds);

        virtual void pre_spawn_stderr_actions(SpawnFileActions & sp);

        virtual void pre_spawn_stdin_actions(SpawnFileActions & sp);

        virtual void pre_spawn_stdout_actions(SpawnFileActions & sp);

    private:
        std::list<ProcessFileHandler *>  io_watchers;
        ProcessStatusWatcher status_watcher;

        void _wait_for_exit_code(bool wait_forever);
};


/* This class is responsible for calling a process, and allows ProcessIO
 * instances to interact with the process's file handles. To do this it demands
 * that they give it a shared pointer to a ProcessFileHandler class (see below).
 */
template<typename... IoClasses>
class Process : public virtual ProcessBase, public IoClasses... {
    public:
        Process(const CommandList & cmds) {
            initialize(cmds);
        }

        ~Process() {
            destroy();
        }
};


class ProcessFileHandler {
    friend class ProcessBase;
    protected:
        virtual ~ProcessFileHandler() {}

        virtual void drain_io(boost::optional<double> seconds) {};

        virtual void post_spawn_actions() = 0;

        virtual void set_eof_actions() {};
};


class StdIn : public ProcessFileHandler, public virtual ProcessBase {
    public:
        StdIn();

        virtual ~StdIn();

        /** Writes to the process's standard input. */
        void write(const char * msg);

        /** Writes to the process's standard input. */
        void write(const char * msg, size_t length);

    protected:

        virtual void post_spawn_actions();

        virtual void pre_spawn_stdin_actions(SpawnFileActions & sp);

        virtual void set_eof_actions();

    private:
        nova::utils::io::Pipe std_in_pipe;
};


class StdErrToFile : public ProcessFileHandler, public virtual ProcessBase {
    public:
        StdErrToFile();

        virtual ~StdErrToFile();

        virtual const char * log_file_name() = 0;

    protected:

        virtual void post_spawn_actions();

        virtual void pre_spawn_stderr_actions(SpawnFileActions & sp);

    private:
        int file_descriptor;
};


class IndependentStdErrAndStdOut : public ProcessFileHandler,
                                   public virtual ProcessBase {
    public:

        struct ReadResult {
            enum FileIndex {
                StdErr = 2,
                StdOut = 1,
                NA = 0
            };

            inline bool err() const {
                return file == StdErr;
            }

            FileIndex file;

            inline bool na() const {
                return file == NA;
            }

            inline bool out() const {
                return file == StdOut;
            }

            size_t write_length;
        };

        IndependentStdErrAndStdOut();

        virtual ~IndependentStdErrAndStdOut();

        /* Reads from either stdour or stderr, and returns the bytes read
         * as well as from which stream reading occurred. */
        ReadResult read_into(char * buffer, const size_t length,
                             boost::optional<double> seconds);

        bool std_err_closed() const {
            return !std_err_pipe.in_is_open();
        }

        bool std_out_closed() const {
            return !std_out_pipe.in_is_open();
        }
    protected:

        virtual void drain_io(boost::optional<double> seconds);

        virtual void post_spawn_actions();

        virtual void pre_spawn_stderr_actions(SpawnFileActions & sp);

        virtual void pre_spawn_stdout_actions(SpawnFileActions & sp);

        // Reads from the last remaing stream.
        ReadResult _read_into(char * buffer, const size_t length,
                              const boost::optional<double> seconds);

        virtual void set_eof_actions();

    private:
        bool draining;
        nova::utils::io::Pipe std_err_pipe;
        nova::utils::io::Pipe std_out_pipe;
};

class StdErrAndStdOut : public ProcessFileHandler, public virtual ProcessBase {
    public:
        StdErrAndStdOut();

        virtual ~StdErrAndStdOut();

        /* Waits until the process's stdout stream has bytes to read or the
         * number of seconds specified by the argument "seconds" passes.
         * If seconds is not set will block here forever (unless a Timer is
         * used).
         * Writes any bytes read to the given argument stream.
         * Returns the number of bytes read (0 for time out).If end of file
         * is encountered the eof property is set to true. */
        size_t read_into(std::stringstream & std_out,
                         const boost::optional<double> seconds=boost::none);
        size_t read_into(char * buffer, const size_t length,
                         const boost::optional<double> seconds=boost::none);

        /** Waits for EOF while reading into the given stream. Note that this
         *  can result in a very large stream as all the standard output is
         *  captured!
         *  Throws TimeOutException if it doesn't happen. */
        void read_into_until_exit(std::stringstream & out, double seconds);

        /* Reads from the process's stdout into the string stream until
         * stdout does not have any data for the given number of seconds.
         * Returns the number of bytes read. If end of file is encountered,
         * sets the eof property to true. */
        size_t read_until_pause(std::stringstream & std_out,
                                const double time_out);

    protected:

        virtual void drain_io(boost::optional<double> seconds);

        virtual void post_spawn_actions();

        virtual void pre_spawn_stderr_actions(SpawnFileActions & sp);

        virtual void pre_spawn_stdout_actions(SpawnFileActions & sp);

        virtual void set_eof_actions();

    private:
        bool draining;
        nova::utils::io::Pipe std_out_pipe;
};


} }  // end nova::process namespace

#endif
