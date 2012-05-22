#ifndef __NOVA_PROCESS_H
#define __NOVA_PROCESS_H


#include <boost/optional.hpp>
#include "nova/Log.h"
#include <list>
#include <sstream>
#include <boost/utility.hpp>
#include <vector>


namespace nova {

class ProcessException : public std::exception {

    public:
        enum Code {
            EXIT_CODE_NOT_ZERO,
            GENERAL,
            NO_PROGRAM_GIVEN,
            PROGRAM_FINISHED,
            SPAWN_FAILURE
        };

        ProcessException(Code code) throw();

        virtual ~ProcessException() throw();

        const Code code;

        virtual const char * what() const throw();
};

/** Runs and manipulate a process by controlling the standard input and output
 *  streams. */
class Process : private boost::noncopyable {

    public:
        typedef std::list<const char *> CommandList;

        Process(const CommandList & cmds, bool wait_for_close=false);

        ~Process();

        /** True if the process has ended. */
        inline bool eof() const {
            return eof_flag;
        }

        /** Executes the given command, waiting until its finished. Returns
         *  true if the command runs successfully with a good exit code, false
         *  otherwise. */
        static void execute(const CommandList & cmds, double time_out=30);

        /** Similar to execute, but throws a TimeOutException if any reads take
         *  longer than the time_out argument. */
        static void execute(std::stringstream & out, const CommandList & cmds,
                            double time_out=30);

        /** Executes the given command but does not wait until its finished.
         *  This function is an anomaly because unlike most functions of this
         *  class it does not open up the process's streams or wait for it. */
        static pid_t execute_and_abandon(const CommandList & cmds);

        /** Returns true if the given pid is alive. */
        static bool is_pid_alive(pid_t pid);

        /* Waits until the process's stdout stream has bytes to read or the
         * number of seconds specified by the argument "seconds" passes.
         * If seconds is not set will block here forever (unless a Timer is
         * used).
         * Writes any bytes read to the given argument stream.
         * Returns the number of bytes read (0 for time out).If end of file
         * is encountered the eof property is set to true. */
        size_t read_into(std::stringstream & std_out,
                         const boost::optional<double> seconds=boost::none);

        /* Reads from the process's stdout into the string stream until
         * stdout does not have any data for the given number of seconds.
         * Returns the number of bytes read. If end of file is encountered,
         * sets the eof property to true. */
        size_t read_until_pause(std::stringstream & std_out,
                                const double pause_time, const double time_out);

        /** This is always false until eof() returns true. */
        inline bool successful() const {
            return success;
        }

        /** Waits for EOF, throws TimeOutException if it doesn't happen. */
        void wait_for_eof(double seconds);

        /** Waits for EOF while reading into the given stream.
         *  Throws TimeOutException if it doesn't happen. */
        void wait_for_eof(std::stringstream & out, double seconds);

        /** Writes to the process's standard input. */
        void write(const char * msg);

        /** Writes to the process's standard input. */
        void write(const char * msg, size_t length);

    private:

        /** Helper class for managing the file descriptors of a pipe.*/
        class Pipe : private boost::noncopyable
        {
            public:
                Pipe();

                ~Pipe();

                void close_in();

                void close_out();

                inline int in() {
                    return fd[0];
                }

                inline int out() {
                    return fd[1];
                }

            private:

                int fd[2];
                bool is_open[2];

                void close(int index);
        };

        const char * const * argv;
        bool eof_flag;
        pid_t pid;
        Pipe std_in_pipe;
        Pipe std_out_pipe;
        bool success;
        bool wait_for_close;

        void create_argv(char * * & new_argv, int & new_argv_length,
                         const CommandList & cmds);

        void delete_argv(char * * & new_argv, int & new_argv_length);

        // Returns true when the input file descriptor is ready.
        bool ready(int file_desc, const boost::optional<double> seconds);

        void set_eof();
};

}  // end nova namespace

#endif
