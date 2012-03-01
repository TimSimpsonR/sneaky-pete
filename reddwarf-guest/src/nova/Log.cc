#include "nova/Log.h"

#include <boost/format.hpp>
#include <iostream>
#include <boost/optional.hpp>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/stat.h>
#include <time.h>
#include "nova/guest/utils.h"

using boost::format;
using nova::Log;
using nova::guest::utils::IsoDateTime;
using boost::optional;
using std::string;

namespace nova {

namespace {

    const char * level_to_string(Log::Level level) {
        switch(level) {
            case Log::LEVEL_DEBUG:
                return "DEBUG";
            case Log::LEVEL_ERROR:
                return "ERROR";
            case Log::LEVEL_INFO:
                return "INFO ";
            default:
                return "UNKNOWN!";
        }
    }

    static boost::mutex global_mutex;

    static time_t start_time;

} // end anonymous namespace


/**---------------------------------------------------------------------------
 *- LogException
 *---------------------------------------------------------------------------*/

const char * LogException::code_to_string(Code code) {
    switch(code) {
        case NOT_INITIALIZED:
            return "The logging system was not initialized.";
        case STRAY_LOG_EXCEPTION:
            return "Log::shutdown() was called; yet somewhere, a log is open.";
        default:
            return "A general logging error occurred.";
    }
}

const char * LogException::what() const throw() {
    return code_to_string(code);
}


/**---------------------------------------------------------------------------
 *- LogFileOptions
 *---------------------------------------------------------------------------*/

LogFileOptions::LogFileOptions(string path, optional<size_t> max_size,
                               optional<double> max_time_in_seconds,
                               int max_old_files)
:   max_old_files(max_old_files),
    max_size(max_size),
    max_time_in_seconds(max_time_in_seconds),
    path(path) {
}


/**---------------------------------------------------------------------------
 *- LogOptions
 *---------------------------------------------------------------------------*/

LogOptions::LogOptions(boost::optional<LogFileOptions> file,
                       bool use_std_streams)
 : file(file), use_std_streams(use_std_streams) {
}

LogOptions LogOptions::simple() {
    LogOptions log_options(boost::none, true);
    return log_options;
}


/**---------------------------------------------------------------------------
 *- LogApiScope
 *---------------------------------------------------------------------------*/

LogApiScope::LogApiScope(const LogOptions & options) {
    Log::initialize(options);
}

LogApiScope::~LogApiScope() {
    Log::shutdown();
}

/**---------------------------------------------------------------------------
 *- Log
 *---------------------------------------------------------------------------*/

void intrusive_ptr_add_ref(Log * ref) {
    boost::lock_guard<boost::mutex> lock(ref->mutex);
    ref->reference_count ++;
}

void intrusive_ptr_release(Log * ref) {
    {
        boost::lock_guard<boost::mutex> lock(ref->mutex);
        ref->reference_count --;
    }
    // Because the global stored in _get_instance() always (*should*) be the
    // last reference holder, this code should not require a lock.
    if (ref->reference_count <= 0) {
        delete ref;
    }
}

Log::Log(const LogOptions & options)
: file(), mutex(), options(options), reference_count(0) {
    open_file();
}

Log::~Log() {
    close_file();
}

void Log::close_file() {
    if (options.file) {
        file.close();
    }
}

boost::optional<size_t> Log::current_log_file_size() {
    if (options.file) {
        struct stat buf;
        if (::stat(options.file.get().path.c_str(), &buf) == 0) {
            return boost::optional<size_t>(buf.st_size);
        } else {
            write(__FILE__, __LINE__, LEVEL_ERROR, "Could not stat log file!");
        }
    }
    return boost::none;
}

void Log::initialize(const LogOptions & options) {
    boost::lock_guard<boost::mutex> lock(global_mutex);
    _open_log(options);
    ::time(&start_time);
}

LogPtr & Log::_get_instance() {
    static LogPtr instance(0);
    return instance;
}

LogPtr Log::get_instance() {
    boost::lock_guard<boost::mutex> lock(global_mutex);
    if (!_get_instance()) {
        std::cerr << "Logging system not initialized!" << std::endl;
        throw LogException(LogException::NOT_INITIALIZED);
    }
    return _get_instance();
}

void Log::open_file() {
    if (options.file) {
        file.open(options.file.get().path.c_str(),
                  std::ios::out | std::ios::app);
    }
}

void Log::_open_log(const LogOptions & options) {
    if (_get_instance() != 0) {
        _get_instance()->write(__FILE__, __LINE__, LEVEL_INFO,
            "UPCOMING EOF: This log will be closed after it finishes its "
            "current task.");
    }
    _get_instance().reset(new Log(options));
}

void Log::rotate_files() {
    boost::lock_guard<boost::mutex> lock(global_mutex);
    LogPtr old = _get_instance();
    if (!old) {
        throw LogException(LogException::NOT_INITIALIZED);
    }
    boost::lock_guard<boost::mutex> lock2(old->mutex);
    if (old->options.file) {
        old->close_file();
        _rotate_files(old->options.file.get());
        old->open_file();
    }
    ::time(&start_time);
}

void Log::rotate_logs_if_needed() {
    LogPtr log = get_instance();
    if (log->should_rotate_logs()) {
        rotate_files();
    }
}

bool Log::should_rotate_logs() {
    if (options.file) {
        const LogFileOptions & file_options = options.file.get();
        if (file_options.max_time_in_seconds) {
            time_t current_time;
            ::time(&current_time);
            double diff = difftime(current_time, start_time);
            if (diff > file_options.max_time_in_seconds.get()) {
                return true;
            }
        } else if (file_options.max_size) {
            boost::optional<size_t> size = current_log_file_size();
            if (size) { // ignore if we couldn't stat the file.
                if (size > file_options.max_size) {
                    return true;
                }
            }
        }
    }
    return false;
}


void Log::write(const char * file_name, int line_number, Log::Level level,
                const char * message)
{
    IsoDateTime time;
    const char * level_string = level_to_string(level);
    boost::lock_guard<boost::mutex> lock(mutex);
    if (options.use_std_streams) {
        std::ostream & out = (level == LEVEL_INFO) ? std::cout : std::cerr;
        out << time.c_str() << " " << level_string << " " << message
            << " for " << file_name <<  ":" << line_number << std::endl;
    }
    {
        if (file.is_open()) {
            file << time.c_str() << " " << level_string << " " << message
                 << " for " << file_name <<  ":" << line_number << std::endl;
            file.flush();

        }
    }
}

LogLine Log::write_fmt(const char * file_name, int line_number,
                       Log::Level level) {
    LogLine line(LogPtr(this), file_name, line_number, level);
    return line;
}

void Log::shutdown() {
    if (_get_instance().get() != 0 && _get_instance()->reference_count > 1) {
        _get_instance()->write_fmt(__FILE__, __LINE__, LEVEL_ERROR)
            ("On shutdown, %d instances of the logger remain.",
             _get_instance()->reference_count);
        throw LogException(LogException::STRAY_LOG_EXCEPTION);
    }
    _get_instance().reset(0);
}

/**---------------------------------------------------------------------------
 *- LogLine
 *---------------------------------------------------------------------------*/

LogLine::LogLine(LogPtr log, const char * file_name, int line_number,
                 Log::Level level)
: file_name(file_name), level(level), line_number(line_number), log(log) {
}

void LogLine::operator()(const char* format, ... ) {
    va_list args;
    va_start(args, format);
    const int BUFF_SIZE = 1024;
    char buf[BUFF_SIZE];
    vsnprintf(buf, BUFF_SIZE, format, args);
    log->write(file_name, line_number, level, buf);
    va_end(args);
}

} // end namespace nova
