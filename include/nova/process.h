#ifndef __NOVA_PROCESS_H
#define __NOVA_PROCESS_H


#include <boost/optional.hpp>
#include "nova/Log.h"
#include <list>
#include <sstream>
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


class Process {

    public:
        typedef std::list<const char *> CommandList;

        Process(const CommandList & cmds, bool wait_for_close=false);

        ~Process();

        inline bool eof() const {
            return eof_flag;
        }

        /** Executes the given command, waiting until its finished. Returns
         *  true if the command runs successfully with a good exit code, false
         *  otherwise. */
        static void execute(const CommandList & cmds, double time_out=30);

        static void execute(std::stringstream & out, const CommandList & cmds,
                            double time_out=30);

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
        void wait_for_eof(std::stringstream & out, double seconds);

        void write(const char * msg);

        void write(const char * msg, size_t length);

    private:
        Process(const Process &);
        Process & operator = (const Process &);

        const char * const * argv;
        bool eof_flag;
        pid_t pid;
        int std_out_fd[2];
        int std_in_fd[2];
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
