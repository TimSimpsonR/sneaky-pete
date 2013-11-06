#ifndef __NOVA_LOG_H
#define __NOVA_LOG_H

#include <fstream>
#include <memory>
#include <boost/format.hpp>
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
        bool show_trace;
        bool use_std_streams;

        LogOptions(boost::optional<LogFileOptions> file, bool use_std_streams,
                   bool show_trace);

        /** Creates a simple set of LogOptions. Useful for tests. */
        static LogOptions simple();
    };

    class LogApiScope : boost::noncopyable  {
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

    class Log : boost::noncopyable  {
        friend void intrusive_ptr_add_ref(Log * ref);
        friend void intrusive_ptr_release(Log * ref);
        friend class LogLine;

        public:
            enum Level {
                LEVEL_DEBUG,
                LEVEL_ERROR,
                LEVEL_INFO,
                LEVEL_TRACE
            };

            boost::optional<size_t> current_log_file_size();

            /* Call this to retrieve an instance of the Logger.
             * The instance returned may need to be taken out of rotation
             * Do *not* store this logger because it may need to be taken out
             * of rotation. */
            static LogPtr get_instance();

            void handle_fmt_error(const char * filename, const int line_number,
                                  const char * fmt_string,
                                  const boost::io::format_error & fe);

            static void initialize(const LogOptions & options);

            /** Saves the current log to name.1, after first renaming all other
             *  backed up logs from 1 - options.max_old_files. */
            static void rotate_files();

            /* Checks if rotation of the logs is necessary given the options
             * and the current state of the log file and the options. */
            static void rotate_logs_if_needed();

            void write(const char * file_name, int line_number,
                       Level level, const char * message);

            template<typename... Types>
            void write(const char * filename, const int line_number,
                       const nova::Log::Level level, const char * fmt_string,
                       const Types... args) {
                try {
                    boost::format fmt = boost::format(fmt_string);
                    const std::string msg(boost::str(decompose_fmt(fmt, args...)));
                    nova::Log::get_instance()->write(filename, line_number, level,
                                                     msg.c_str());
                } catch (const boost::io::format_error & fe) {
                    handle_fmt_error(filename, line_number, fmt_string, fe);
                }
            }

            bool should_rotate_logs();

            static void shutdown();
        protected:

            Log(const LogOptions & options);

            ~Log();

        private:

            void close_file();

            template<typename T>
            static boost::format & decompose_fmt(boost::format & fmt,
                                                 const T & arg) {
                return fmt % arg;
            }

            template<typename HeadType, typename... TailTypes>
            static boost::format & decompose_fmt(
                boost::format & fmt,
                const HeadType & head,
                const TailTypes... tail)
            {
                return decompose_fmt(fmt % head, tail...);
            }

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

}

/* It is necessary to use macros here to automatically insert the file
 * name and line numbers. */
#define NOVA_LOG_DEBUG(fmt, ...) { ::nova::Log::get_instance()->write( \
    __FILE__, __LINE__, nova::Log::LEVEL_DEBUG, fmt, ##__VA_ARGS__); }
#define NOVA_LOG_INFO(fmt, ...) { ::nova::Log::get_instance()->write( \
    __FILE__, __LINE__, nova::Log::LEVEL_INFO, fmt, ##__VA_ARGS__); }
#define NOVA_LOG_ERROR(fmt, ...) { ::nova::Log::get_instance()->write( \
    __FILE__, __LINE__, nova::Log::LEVEL_ERROR, fmt, ##__VA_ARGS__); }
#define NOVA_LOG_TRACE(fmt, ...) { ::nova::Log::get_instance()->write( \
    __FILE__, __LINE__, nova::Log::LEVEL_TRACE, fmt, ##__VA_ARGS__); }

#endif

