#ifndef __NOVA_LOG_H
#define __NOVA_LOG_H

#include <fstream>
#include <memory>
#include <boost/optional.hpp>
#include <boost/smart_ptr.hpp>
#include <string>
#include <boost/thread.hpp>

/**
 * Cheezy mini-logging system.
 */
namespace nova {

    class LogException : public std::exception {
        public:
            enum Code {
                GENERAL,
                NOT_INITIALIZED,
                STRAY_LOG_EXCEPTION
            };

            LogException(Code code) : code(code) {

            }

            const Code code;

            static const char * code_to_string(Code code);

            virtual const char * what() const throw();
    };

    struct LogFileOptions {
        int max_old_files;
        boost::optional<size_t> max_size;
        boost::optional<double> max_time_in_seconds;
        std::string path;
        LogFileOptions(std::string path, boost::optional<size_t> max_size,
                       boost::optional<double> max_time_in_seconds, int max_old_files);

        static void rotate_files();
    };



    struct LogOptions {
        boost::optional<LogFileOptions> file;
        bool use_std_streams;
        LogOptions(boost::optional<LogFileOptions> file, bool use_std_streams);

        /** Creates a simple set of LogOptions. Useful for tests. */
        static LogOptions simple();
    };

    class LogApiScope {
        public:
            LogApiScope(const LogOptions & options);
            ~LogApiScope();
    };

    class Log;

    void intrusive_ptr_add_ref(Log * ref);
    void intrusive_ptr_release(Log * ref);

    typedef boost::intrusive_ptr<Log> LogPtr;

    class LogLine;

    typedef std::auto_ptr<LogLine> LogLinePtr;

    class Log {
        friend void intrusive_ptr_add_ref(Log * ref);
        friend void intrusive_ptr_release(Log * ref);
        friend class LogLine;

        public:
            enum Level {
                LEVEL_DEBUG,
                LEVEL_ERROR,
                LEVEL_INFO
            };

            boost::optional<size_t> current_log_file_size();

            /* Call this to retrieve an instance of the Logger.
             * The instance returned may need to be taken out of rotation
             * Do *not* store this logger because it may need to be taken out
             * of rotation. */
            static LogPtr get_instance();

            static void initialize(const LogOptions & options);

            /** Saves the current log to name.1, after first renaming all other
             *  backed up logs from 1 - options.max_old_files. */
            static void rotate_files();

            /* Checks if rotation of the logs is necessary given the options
             * and the current state of the log file and the options. */
            static void rotate_logs_if_needed();

            void write(const char * file_name, int line_number,
                       Level level, const char * message);

            LogLine write_fmt(const char * file_name, int line_number,
                              Level level);

            bool should_rotate_logs();

            static void shutdown();
        protected:

            Log(const LogOptions & options);

            ~Log();

        private:
            Log(const Log &);
            Log & operator = (const Log &);

            void close_file();

            static LogPtr & _get_instance();

            std::ofstream file;

            boost::mutex mutex;

            void open_file();

            static void _open_log(const LogOptions & options);

            const LogOptions options;

            int reference_count;

            static void _rotate_files(LogFileOptions options);
    };

    class LogLine {
        public:
            LogLine(LogPtr log, const char * file_name, int line_number,
                    Log::Level level);

            void operator()(const char* format, ... );

        private:
            const char * file_name;

            Log::Level level;

            int line_number;

            LogPtr log;
    };



    /* For repeated calls to the log, grab a Log instance once and then use
     * this. */
    #define NOVA_LOG_WRITE(log, level) log->write_fmt(__FILE__, __LINE__, \
                                                      nova::Log::level)

    /** These Macros call get_instance() and give the line numbers. */
    #define NOVA_LOG_DEBUG(arg) { nova::Log::get_instance()->write( \
        __FILE__, __LINE__, nova::Log::LEVEL_DEBUG, arg); }
    #define NOVA_LOG_INFO(arg) { nova::Log::get_instance()->write( \
        __FILE__, __LINE__, nova::Log::LEVEL_INFO, arg); }
    #define NOVA_LOG_ERROR(arg) { nova::Log::get_instance()->write( \
        __FILE__, __LINE__, nova::Log::LEVEL_ERROR, arg); }

    #define NOVA_LOG_DEBUG2 nova::Log::get_instance()->write_fmt( \
        __FILE__, __LINE__, nova::Log::LEVEL_DEBUG)
    #define NOVA_LOG_INFO2 nova::Log::get_instance()->write_fmt( \
        __FILE__, __LINE__, nova::Log::LEVEL_INFO)
    #define NOVA_LOG_ERROR2 nova::Log::get_instance()->write_fmt( \
        __FILE__, __LINE__, nova::Log::LEVEL_ERROR)
}

#endif

