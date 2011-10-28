#include "nova/guest/apt.h"


#include <boost/assign/list_of.hpp>
#include "nova/Log.h"
#include <errno.h>
#include <boost/foreach.hpp>
#include <boost/format.hpp>
#include <fstream>
#include <iostream>
#include <malloc.h>  // Valgrind complains if we don't use "free" below. ;_;
#include "nova/process.h"
#include "nova/utils/io.h"
#include "nova/utils/regex.h"
#include <sys/select.h>
#include <boost/smart_ptr.hpp>
#include <spawn.h>
#include <sstream>
#include <stdio.h>
#include <string.h>
#include <vector>


extern char **environ;

using namespace boost::assign;
using std::auto_ptr;
using boost::format;
using boost::optional;
using nova::Process;
using nova::utils::Regex;
using nova::utils::RegexMatches;
using nova::utils::RegexMatchesPtr;
using boost::shared_ptr;
using std::stringstream;
using std::string;
using nova::utils::io::TimeOutException;
using nova::utils::io::Timer;
using std::vector;

#ifdef _VERBOSE_NOVA_GUEST_APT
#define VERBOSE_LOG(args) log.debug(args)
#define VERBOSE_LOG3(a1, a2, a3) log.debug(a1, a2, a3)
#else
#define VERBOSE_LOG(args) /* args */
#define VERBOSE_LOG3(a1, a2, a3) /* a1, a2, a3 */
#endif

namespace nova { namespace guest { namespace apt {

enum OperationResult {
    OK = 0,
    RUN_DPKG_FIRST = 1,
    REINSTALL_FIRST = 2
};

//TODO(tim.simpson): Make this the actual standard.
#define PACKAGE_NAME_REGEX "\\S+"

void fix(double time_out) {
    // sudo -E dpkg --configure -a
    Process process(list_of("/usr/bin/sudo")("-E")("dpkg")("--configure")("-a"),
                    false);

    // Expect just a simple EOF.
    stringstream std_out;
    int bytes_read = process.read_until_pause(std_out, time_out, time_out);
    if (!process.eof()) {
        throw AptException(AptException::COULD_NOT_FIX);
    }
}

struct ProcessResult {
    int index;
    RegexMatchesPtr matches;
};

// Returns -1 on EOF, an index of a regular expression that matched.
optional<ProcessResult> match_output(Process & process,
                                     const vector<string> & patterns,
                                     double seconds)
{
    Log log;
    typedef shared_ptr<Regex> RegexPtr;
    vector<RegexPtr> regexes;
    BOOST_FOREACH(const string & pattern, patterns) {
        RegexPtr ptr(new Regex(pattern.c_str()));
        regexes.push_back(ptr);
    }
    stringstream std_out;

    Timer timer(seconds);
    while(!process.eof()) {
        size_t count = process.read_into(std_out, boost::none);
        VERBOSE_LOG("********************************************************");
        VERBOSE_LOG(std_out.str().c_str());
        VERBOSE_LOG("********************************************************");
        if (count == 0) {
            if (!process.eof()) {
                log.error("read should not exit until it gets data.");
                throw AptException(AptException::GENERAL);
            }
        }
        string output = std_out.str();
        for (size_t index = 0; index < regexes.size(); index ++) {
            RegexPtr & regex = regexes[index];;
            RegexMatchesPtr matches = regex->match(output.c_str());
            VERBOSE_LOG("____________________________________________________");
            VERBOSE_LOG(output.c_str());
            VERBOSE_LOG3("Trying to match %s against regex %s", output.c_str(),
                        patterns[index].c_str());
            VERBOSE_LOG("____________________________________________________");
            if (!!matches) {
                ProcessResult result;
                result.index = index;
                result.matches = matches;
                return optional<ProcessResult>(result);
            }
        }
        // Nothing matches... let's try again until time_out or eof().
    }
    return boost::none;
}

/**
 *  Attempts to install a package.
 *  Returns OK if the package installs fine or a result code if a
 *  recoverable-error occurred.
 *  Raises an exception if a non-recoverable error or time out occurs.
 */
OperationResult _install(const char * package_name, double time_out) {
    Log log;
    Process process(list_of("/usr/bin/sudo")("-E")
                    ("DEBIAN_FRONTEND=noninteractive")("apt-get")("-y")
                    ("--allow-unauthenticated")("install")(package_name),
                    false);

    vector<string> patterns;
    // 0 = permissions issue
    patterns.push_back(".*password*");
    // 1 - 2 = could not find package
    patterns.push_back(str(format("E: Unable to locate package %s")
                           % package_name));
    patterns.push_back(str(format("Couldn't find package %s")
                           % package_name));
    // 3 = need to fix
    patterns.push_back("dpkg was interrupted, you must manually run "
                       "'sudo dpkg --configure -a'");
    // 4 = lock error
    patterns.push_back("Unable to lock the administration directory");
    // 5 - 6 = Success, but only if followed up by EOF.
    patterns.push_back("Setting up (" PACKAGE_NAME_REGEX ")\\s+");
    patterns.push_back("is already the newest version");

    optional<ProcessResult> result;
    try  {
        result = match_output(process, patterns, time_out);
    } catch(const TimeOutException & toe) {
        throw AptException(AptException::PROCESS_TIME_OUT);
    }
    if (!!result) { // eof
        const int index = result.get().index;
        if (index == 0) {
            throw AptException(AptException::PERMISSION_ERROR);
        } else if (index == 1 || index == 2) {
            log.error2("Could not find package %s.", package_name);
            throw AptException(AptException::PACKAGE_NOT_FOUND);
        } else if (index == 3) {
            return RUN_DPKG_FIRST;
        } else if (index == 4) {
            throw AptException(AptException::ADMIN_LOCK_ERROR);
        } else {
            if (index == 5) {
                string output_package_name = result.get().matches->get(1);
                if (output_package_name != package_name) {
                    log.error2("Wait, saw 'Setting up' but it wasn't our "
                               "package! %s != %s", package_name,
                               output_package_name.c_str());
                    throw AptException(AptException::GENERAL);
                }
            }
            try {
                process.wait_for_eof(time_out);
            } catch(const TimeOutException & toe) {
                throw AptException(AptException::PROCESS_TIME_OUT);
            }
            return OK;
        }
    }
    log.error("Got EOF calling apt-get install!");
    throw AptException(AptException::GENERAL);
}

void install(const char * package_name, const double time_out) {
    Log log;
    OperationResult result = _install(package_name, time_out);
    if (result != OK) {
        if (result == RUN_DPKG_FIRST) {
            fix(time_out);
        }
        result = _install(package_name, time_out);
        if (result != OK) {
            log.error2("Package %s is in a bad state.", package_name);
            throw AptException(AptException::PACKAGE_STATE_ERROR);
        }
    }
}

OperationResult _remove(const char * package_name, double time_out) {
    Log log;
    Process process(list_of("/usr/bin/sudo")("-E")("apt-get")("-y")
                    ("--allow-unauthenticated")("remove")(package_name), false);

    vector<string> patterns;
    // 0 = permissions issue
    patterns.push_back(".*password*");
    // 1 -2 = Package not found
    patterns.push_back(str(format("E: Unable to locate package %s")
                           % package_name));
    patterns.push_back("Couldn't find package");
    // 3 - 4 = Reinstall first
    patterns.push_back("Package is in a very bad inconsistent state");
    patterns.push_back("Sub-process /usr/bin/dpkg returned an error code");
    // 5 = need to fix
    patterns.push_back("dpkg was interrupted, you must manually run "
                       "'sudo dpkg --configure -a'");
    // 6 = lock error
    patterns.push_back("Unable to lock the administration directory");
    // 7 - 8 = Success, but the captured string must be our package followed by EOF.
    patterns.push_back("Removing (" PACKAGE_NAME_REGEX ")");
    patterns.push_back("Package (" PACKAGE_NAME_REGEX ") is not installed, "
                       "so not removed");

    optional<ProcessResult> result;
    try  {
        result = match_output(process, patterns, time_out);
    } catch(const TimeOutException & toe) {
        throw AptException(AptException::PROCESS_TIME_OUT);
    }
    if (!!result) { // eof
        const int index = result.get().index;
        if (index == 0) {
            throw AptException(AptException::PERMISSION_ERROR);
        } else if (index == 1 || index == 2) {
            log.error2("Could not find package %s.", package_name);
            throw AptException(AptException::PACKAGE_NOT_FOUND);
        } else if (index == 3 || index == 4) {
            return REINSTALL_FIRST;
        } else if (index == 5) {
            return RUN_DPKG_FIRST;
        } else if (index == 6) {
            throw AptException(AptException::ADMIN_LOCK_ERROR);
        } else {
            if (index == 7 || index == 8) {
                string output_package_name = result.get().matches->get(1);
                if (output_package_name != package_name) {
                    log.error2("Wait, saw 'Setting up' but it wasn't our "
                               "package! %s != %s", package_name,
                               output_package_name.c_str());
                    throw AptException(AptException::GENERAL);
                }
            }
            try {
                process.wait_for_eof(time_out);
            } catch(const TimeOutException & toe) {
                throw AptException(AptException::PROCESS_TIME_OUT);
            }
            return OK;
        }
    }
    log.error("Got EOF calling apt-get remove!");
    throw AptException(AptException::GENERAL);
}

void remove(const char * package_name, const double time_out) {
    OperationResult result = _remove(package_name, time_out);
    if (result != OK) {
        if (result == REINSTALL_FIRST) {
            _install(package_name, time_out);
        } else if (result == RUN_DPKG_FIRST) {
            fix(time_out);
        }
        result = _remove(package_name, time_out);
        if (result != OK) {
            throw AptException(AptException::PACKAGE_STATE_ERROR);
        }
    }
}

typedef boost::optional<std::string> optional_string;

optional<string> version(const char * package_name, const double time_out) {
    Log log;
    Process process(list_of("/usr/bin/dpkg")("-l")(package_name), false);

    vector<string> patterns;
    // 0 = Not found
    patterns.push_back("No packages found matching (" PACKAGE_NAME_REGEX
                       ")\\.");
    // 1 = success
    patterns.push_back("\n\\w\\w\\s+(" PACKAGE_NAME_REGEX
                       ")\\s+(\\S+)\\s+(.*)$");
    optional<ProcessResult> result;
    try  {
        result = match_output(process, patterns, time_out);
    } catch(const TimeOutException & toe) {
        throw AptException(AptException::PROCESS_TIME_OUT);
    }
    if (!!result) {
        // 0 and 1 should return the package name as the first regex match.
        string output_package_name = result.get().matches->get(1);
        if (result.get().matches->get(1) != package_name) {
            log.error2("dpkg called the package something different. "
                      "%s != %s", package_name, output_package_name.c_str());
            throw AptException(AptException::GENERAL);
        }
        if (result.get().index == 0) {
            return boost::none;
        }
        else if (result.get().index == 1) {
            string version = result.get().matches->get(2);
            if (version == "<none>") {
                return boost::none;
            } else {
                return optional<string>(version);
            }
        }
    }
    log.error("version() saw unexpected output from dpkg!");
    throw AptException(AptException::UNEXPECTED_PROCESS_OUTPUT);
}

} } }  // end namespace nova::guest::apt
