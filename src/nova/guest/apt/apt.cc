#include "pch.hpp"
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
using std::ofstream;
using boost::optional;
namespace proc = nova::process;
using nova::utils::Regex;
using nova::utils::RegexMatches;
using nova::utils::RegexMatchesPtr;
using boost::shared_ptr;
using std::stringstream;
using std::string;
using nova::utils::io::TimeOutException;
using nova::utils::io::Timer;
using std::vector;


//TODO(tim.simpson): Make this the actual standard.
#define PACKAGE_NAME_REGEX "\\S+"

namespace nova { namespace guest { namespace apt {

namespace {

    #ifdef NOVA_GUEST_WHEEZY
        typedef proc::Process<proc::StdOutOnly> StreamingProcType;
    #else
        typedef proc::Process<proc::StdErrAndStdOut> StreamingProcType;
    #endif

    enum OperationResult {
        OK = 0,
        RUN_DPKG_FIRST = 1,
        REINSTALL_FIRST = 2
    };

    struct ProcessResult {
        int index;
        RegexMatchesPtr matches;
    };

    // Strip quotes off string if found.
    string strip_quotes(const string & value) {
        auto result = value;
        if (result.length() > 1 &&
            ((result[0] == '"'  && result[result.length() - 1] == '"') ||
             (result[0] == '\'' && result[result.length() - 1] == '\''))) {
            result.erase(0, 1);
            result.erase(result.length() - 1, 1);
        }
        return result;
    }

    void wait_for_proc_to_finish(pid_t pid, int time_out) {
        int time_left = time_out;
        while (proc::is_pid_alive(pid) && time_left > 0) {
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
    proc::CommandList cmds;
    if (with_sudo) {
        cmds += "/usr/bin/sudo", "-E";
    }
    cmds += "/usr/bin/dpkg", "--configure", "-a";
    proc::Process<proc::StdErrAndStdOut> process(cmds);

    // Expect just a simple EOF.
    stringstream std_out;
    process.read_until_pause(std_out, time_out);
    if (!process.is_finished()) {
        throw AptException(AptException::COULD_NOT_FIX);
    }
}


// Returns -1 on EOF, an index of a regular expression that matched.
template<typename ProcessClass>
optional<ProcessResult> match_output(
    ProcessClass & process,
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
    while(!process.is_finished()) {
        auto count = process.read_into(std_out, seconds);
        NOVA_LOG_TRACE("******************************************************");
        NOVA_LOG_TRACE(std_out.str().c_str());
        NOVA_LOG_TRACE("******************************************************");
        if (count == 0) {
            //NOTE(tim.simpson): Return boost::none here if we hit the timeout?
            //                   Kevin reports this makes the code work.
            if (!process.is_finished()) {
                NOVA_LOG_ERROR("read should not exit until it gets data.");
                throw AptException(AptException::GENERAL);
            }
            return boost::none;
        }
        string output = std_out.str();
        for (size_t index = 0; index < regexes.size(); index ++) {
            RegexPtr & regex = regexes[index];;
            RegexMatchesPtr matches = regex->match(output.c_str());
            NOVA_LOG_TRACE("__________________________________________________");
            NOVA_LOG_TRACE(output.c_str());
            NOVA_LOG_TRACE("Trying to match %s against regex %s",
                           output, patterns[index]);
            NOVA_LOG_TRACE("__________________________________________________");
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

void AptGuest::install(const char * package_name, const double time_out) {
    update(time_out);
    string cmd = "/usr/bin/sudo -E /usr/bin/apt-get -y --allow-unauthenticated install ";
    cmd += package_name;
    setenv("DEBIAN_FRONTEND", "noninteractive", 1);
    try {
        proc::shell(cmd.c_str());
    } catch(const proc::ProcessException & err) {
        fix(time_out);
        proc::shell(cmd.c_str());
    }
}

pid_t AptGuest::_install_new_self() {
    // Calling the update function above won't work when updating this process,
    // because there will be broken pipe issues in apt-get when this program
    // exits.
    proc::CommandList cmds;
    if (with_sudo) {
        cmds += "/usr/bin/sudo", "-E";
    }
    setenv("DEBIAN_FRONTEND", "noninteractive", 1);
    cmds += "/usr/bin/apt-get", "-y", "--allow-unauthenticated", "install",
            self_package_name.c_str();
    try {
        return proc::execute_and_abandon(cmds);
    } catch(const proc::ProcessException & pe) {
        NOVA_LOG_ERROR("An error occurred calling apt-get update:%s",
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
    if (proc::is_pid_alive(pid)) {
        NOVA_LOG_ERROR("Time out. apt-get is still running. This is very bad.");
        throw AptException(AptException::PROCESS_TIME_OUT);
    }
    NOVA_LOG_INFO("The install program finished but Sneaky Pete persists. "
                  "Assuming no update of this code was needed.");
}


void _call_remove(const char * package_name, double time_out) {

    string cmd = "/usr/bin/sudo -E /usr/bin/apt-get -y --allow-unauthenticated remove ";
    cmd += package_name;
    setenv("DEBIAN_FRONTEND", "noninteractive", 1);
    proc::shell(cmd.c_str());

}

void AptGuest::resilient_remove(const char * package_name,
                                const double time_out) {
    try {
        _call_remove(package_name, time_out);
    } catch(const proc::ProcessException & err) {
        install(package_name, time_out);
        _call_remove(package_name, time_out);
    }
}

void AptGuest::remove(const char * package_name, const double time_out) {
    resilient_remove(package_name, time_out);
}

void AptGuest::update(const optional<double> time_out) {
    NOVA_LOG_INFO("Calling apt-get update...");
    try {
        string cmd = "/usr/bin/sudo -E /usr/bin/apt-get update";
        setenv("DEBIAN_FRONTEND", "noninteractive", 1);
        proc::shell(cmd.c_str());
    } catch(const TimeOutException & toe) {
        throw AptException(AptException::PROCESS_TIME_OUT);
    } catch(const proc::ProcessException & pe) {
        NOVA_LOG_ERROR("An error occurred calling apt-get update:%s",
                       pe.what());
        throw AptException(AptException::UPDATE_FAILED);
    }
}


typedef boost::optional<std::string> optional_string;

optional<string> AptGuest::version(const char * package_name,
                                   const double time_out) {
    NOVA_LOG_DEBUG("Getting version of %s", package_name);
    proc::CommandList cmds = list_of("/usr/bin/dpkg-query")("-W")(package_name);
    proc::Process<proc::StdErrAndStdOut> process(cmds);

    vector<string> patterns;
    // 0 = Not found in Squeeze: looks like:
    //      No packages found matching cowsay
    patterns.push_back("No packages found matching (" PACKAGE_NAME_REGEX
                       ")\\.");
    // 1 = Not found in Wheezy: looks like:
    //      dpkg-query: no packages found matching cowsay
    patterns.push_back("no packages found matching ("
                        PACKAGE_NAME_REGEX ")");
    // 2 = success: looks like:
    //      cowsay  0.0.1-placeholder
    patterns.push_back("(" PACKAGE_NAME_REGEX ")\\s+(\\S*).*");
    optional<ProcessResult> result;
    try  {
        result = match_output(process, patterns, time_out);
    } catch(const TimeOutException & toe) {
        throw AptException(AptException::PROCESS_TIME_OUT);
    }
    if (!!result) {
        // 0 and 1 should return the package name as the first regex match.
        string output_package_name = result.get().matches->get(1);
        NOVA_LOG_DEBUG("Match=%d", result.get().index);
        NOVA_LOG_DEBUG("Got the output_package_name: %s", output_package_name.c_str());
        if (result.get().matches->get(1) != package_name) {
            NOVA_LOG_ERROR("dpkg called the package something different. "
                        "%s != %s", package_name, output_package_name.c_str());
            throw AptException(AptException::GENERAL);
        }
        NOVA_LOG_DEBUG("Got the result index: %d", result.get().index);
        if (result.get().index == 0 || result.get().index == 1) {
            return boost::none;
        }
        else if (result.get().index == 2) {
            string version = result.get().matches->get(2);
            NOVA_LOG_DEBUG("Got the version: %s", version.c_str());
            if (version == "<none>" || version.empty()) {
                NOVA_LOG_DEBUG("No version found.");
                return boost::none;
            } else {
                NOVA_LOG_DEBUG("Returning version %s.", version.c_str());
                return optional<string>(version);
            }
        }
    }
    NOVA_LOG_ERROR("version() saw unexpected output from dpkg!");
    throw AptException(AptException::UNEXPECTED_PROCESS_OUTPUT);
}

void AptGuest::write_file(const char * name, const char * file_ext,
                          const optional<string> & file_contents,
                          const optional<double> time_out) {
    if (!file_contents) {
        NOVA_LOG_INFO("No %s file given.", name);
        return;
    }
    NOVA_LOG_INFO("Writing new %s file.", name);
    ofstream file("/tmp/cdb");
    file.exceptions(ofstream::failbit | ofstream::badbit);
    if (!file.good()) {
        throw AptException(AptException::ERROR_WRITING_PREFERENCES);
    }
    file << file_contents.get();
    file.close();
    NOVA_LOG_INFO("Copying new %s file into place.", name);
    string file_name = str(format("/etc/apt/%s%s.d/cdb") % name % file_ext);
    process::execute(list_of("/usr/bin/sudo")("cp")("/tmp/cdb")
                            (file_name.c_str()));
}

void AptGuest::write_repo_files(const optional<string> & preferences_file,
                                const optional<string> & sources_file,
                                const optional<double> time_out) {
    write_file("preferences", "", preferences_file, time_out);
    write_file("sources", ".list", sources_file, time_out);
    update();
}

} } }  // end namespace nova::guest::apt
