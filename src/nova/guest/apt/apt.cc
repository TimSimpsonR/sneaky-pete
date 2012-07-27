#include "nova/guest/apt.h"


#include <boost/assign/list_of.hpp>
#include <boost/assign/std/list.hpp>
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
#include <stdlib.h>
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
#define VERBOSE_LOG(args) NOVA_LOG_DEBUG(args)
#define VERBOSE_LOG3(a1, a2, a3) NOVA_LOG_DEBUG2(a1, a2, a3)
#else
#define VERBOSE_LOG(args) /* args */
#define VERBOSE_LOG3(a1, a2, a3) /* a1, a2, a3 */
#endif


//TODO(tim.simpson): Make this the actual standard.
#define PACKAGE_NAME_REGEX "\\S+"

namespace nova { namespace guest { namespace apt {

namespace {

    enum OperationResult {
        OK = 0,
        RUN_DPKG_FIRST = 1,
        REINSTALL_FIRST = 2
    };

    struct ProcessResult {
        int index;
        RegexMatchesPtr matches;
    };

    void wait_for_proc_to_finish(pid_t pid, int time_out) {
        int time_left = time_out;
        while (Process::is_pid_alive(pid) && time_left > 0) {
            boost::this_thread::sleep(boost::posix_time::seconds(1));
            time_left--;
        }
    }

} // end anonymous namespace


AptGuest::AptGuest(bool with_sudo, const char * self_package_name,
                   int self_update_time_out)
: self_package_name(self_package_name),
  self_update_time_out(self_update_time_out), with_sudo(with_sudo)
{
}

void AptGuest::fix(double time_out) {
    // sudo -E dpkg --configure -a
    Process::CommandList cmds;
    if (with_sudo) {
        cmds += "/usr/bin/sudo", "-E";
    }
    cmds += "/usr/bin/dpkg", "--configure", "-a";
    Process process(cmds, false);

    // Expect just a simple EOF.
    stringstream std_out;
    process.read_until_pause(std_out, time_out, time_out);
    if (!process.eof()) {
        throw AptException(AptException::COULD_NOT_FIX);
    }
}


// Returns -1 on EOF, an index of a regular expression that matched.
optional<ProcessResult> match_output(Process & process,
                                     const vector<string> & patterns,
                                     double seconds)
{
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
                NOVA_LOG_ERROR("read should not exit until it gets data.");
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
OperationResult _install(bool with_sudo, const char * package_name,
                         double time_out) {
    Process::CommandList cmds;
    if (with_sudo) {
        cmds += "/usr/bin/sudo", "-E";
    }
    setenv("DEBIAN_FRONTEND", "noninteractive", 1);
    cmds += "/usr/bin/apt-get", "-y", "--allow-unauthenticated", "install",
            package_name;
    Process process(cmds, false);

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
    patterns.push_back(str(format("Setting up %s") % package_name));
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
            NOVA_LOG_ERROR2("Could not find package %s.", package_name);
            throw AptException(AptException::PACKAGE_NOT_FOUND);
        } else if (index == 3) {
            return RUN_DPKG_FIRST;
        } else if (index == 4) {
            throw AptException(AptException::ADMIN_LOCK_ERROR);
        } else {
            try {
                process.wait_for_eof(time_out);
            } catch(const TimeOutException & toe) {
                throw AptException(AptException::PROCESS_TIME_OUT);
            }
            return OK;
        }
    }
    NOVA_LOG_ERROR("Got EOF calling apt-get install!");
    throw AptException(AptException::GENERAL);
}

void AptGuest::install(const char * package_name, const double time_out) {
    update(time_out);
    OperationResult result = _install(with_sudo, package_name, time_out);
    if (result != OK) {
        if (result == RUN_DPKG_FIRST) {
            fix(time_out);
        }
        result = _install(with_sudo, package_name, time_out);
        if (result != OK) {
            NOVA_LOG_ERROR2("Package %s is in a bad state.", package_name);
            throw AptException(AptException::PACKAGE_STATE_ERROR);
        }
    }
}

pid_t AptGuest::_install_new_self() {
    // Calling the update function above won't work when updating this process,
    // because there will be broken pipe issues in apt-get when this program
    // exits.
    Process::CommandList cmds;
    if (with_sudo) {
        cmds += "/usr/bin/sudo", "-E";
    }
    setenv("DEBIAN_FRONTEND", "noninteractive", 1);
    cmds += "/usr/bin/apt-get", "-y", "--allow-unauthenticated", "install",
            self_package_name.c_str();
    try {
        return Process::execute_and_abandon(cmds);
    } catch(const ProcessException & pe) {
        NOVA_LOG_ERROR2("An error occurred calling apt-get update:%s",
                        pe.what());
        throw AptException(AptException::UPDATE_FAILED);
    }
}

void AptGuest::install_self_update() {
    NOVA_LOG_INFO("Installing a new version of Sneaky Pete...");
    update(self_update_time_out);
    pid_t pid = _install_new_self();
    NOVA_LOG_INFO("Waiting for oblivion...");
    wait_for_proc_to_finish(pid, self_update_time_out);
    if (Process::is_pid_alive(pid)) {
        NOVA_LOG_ERROR("Time out. apt-get is still running. This is very bad.");
        throw AptException(AptException::PROCESS_TIME_OUT);
    }
    NOVA_LOG_INFO("The install program finished but Sneaky Pete persists. "
                  "Assuming no update of this code was needed.");
}


OperationResult _remove(bool with_sudo, const char * package_name,
                        double time_out) {
    Process::CommandList cmds;
    if (with_sudo) {
        cmds += "/usr/bin/sudo", "-E";
    }
    cmds += "/usr/bin/apt-get", "-y", "--allow-unauthenticated", "remove", package_name;
    Process process(cmds, false);

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
            NOVA_LOG_ERROR2("Could not find package %s.", package_name);
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
                    NOVA_LOG_ERROR2("Wait, saw 'Setting up' but it wasn't our "
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
    NOVA_LOG_ERROR("Got EOF calling apt-get remove!");
    throw AptException(AptException::GENERAL);
}

void AptGuest::remove(const char * package_name, const double time_out) {
    OperationResult result = _remove(with_sudo, package_name, time_out);
    if (result != OK) {
        if (result == REINSTALL_FIRST) {
            _install(with_sudo, package_name, time_out);
        } else if (result == RUN_DPKG_FIRST) {
            fix(time_out);
        }
        result = _remove(with_sudo, package_name, time_out);
        if (result != OK) {
            throw AptException(AptException::PACKAGE_STATE_ERROR);
        }
    }
}

void AptGuest::update(const double time_out) {
    Process::CommandList cmds;
    if (with_sudo) {
        cmds += "/usr/bin/sudo", "-E";
    }
    cmds += "/usr/bin/apt-get", "update";
    try {
        Process::execute(cmds, time_out);
    } catch(const TimeOutException & toe) {
        throw AptException(AptException::PROCESS_TIME_OUT);
    } catch(const ProcessException & pe) {
        NOVA_LOG_ERROR2("An error occurred calling apt-get update:%s",
                        pe.what());
        throw AptException(AptException::UPDATE_FAILED);
    }
}


typedef boost::optional<std::string> optional_string;

optional<string> AptGuest::version(const char * package_name,
                                   const double time_out) {
    NOVA_LOG_DEBUG2("Getting version of %s", package_name);
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
            NOVA_LOG_ERROR2("dpkg called the package something different. "
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
    NOVA_LOG_ERROR("version() saw unexpected output from dpkg!");
    throw AptException(AptException::UNEXPECTED_PROCESS_OUTPUT);
}

} } }  // end namespace nova::guest::apt
