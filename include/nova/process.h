#ifndef __NOVA_PROCESS_H
#define __NOVA_PROCESS_H

#include <boost/optional.hpp>
#include "nova/log.h"
#include <sstream>


namespace nova {

class ProcessException : public std::exception {

    public:
        enum Code {
            GENERAL,
            PROGRAM_FINISHED,
            SPAWN_FAILURE
        };

        ProcessException(Code code) throw();

        virtual ~ProcessException() throw();

        virtual const char * what() const throw();

    private:
        Code code;
};


class Process {

    public:
        Process(const char * program_path, const char * const argv[]);

        ~Process();

        inline bool eof() const {
            return eof_flag;
        }

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

        /** Waits for EOF, throws TimeOutException if it doesn't happen. */
        void wait_for_eof(double seconds);

        void write(const char * msg);

        void write(const char * msg, size_t length);

    private:
        const char * const * argv;
        bool eof_flag;
        Log log;
        pid_t pid;
        const char * program_path;
        int std_out_fd[2];
        int std_in_fd[2];

        void create_argv(char * * & new_argv, int & new_argv_length,
                         const char * program_path,
                         const char * const argv[]);

        void delete_argv(char * * & new_argv, int & new_argv_length);

        // Returns true when the input file descriptor is ready.
        bool ready(int file_desc, const boost::optional<double> seconds);

        void set_eof();
};

}  // end nova namespace

#endif
