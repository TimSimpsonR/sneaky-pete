#include <boost/foreach.hpp>
#include <boost/thread/thread.hpp>
#include "nova/flags.h"
#include "nova/Log.h"
#include "nova/LogFlags.h"
#include "nova/guest/apt.h"


using nova::guest::apt::AptGuest;


void sad() {
    NOVA_LOG_ERROR("^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^");
    NOVA_LOG_ERROR("^                                       _    ^");
    NOVA_LOG_ERROR("^  Could not launch         \?\?! >   O  / /   ^");
    NOVA_LOG_ERROR("^           program...                | |    ^");
    NOVA_LOG_ERROR("^                                   O  \\_\\   ^");
    NOVA_LOG_ERROR("^                                            ^");
    NOVA_LOG_ERROR("^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^");
}

void run(const char * program, int argc, char* argv[]) {
    char * * argv_copy = new char *[argc + 1];
    for (int index = 0; index < argc; ++ index) {
        argv_copy[index] = argv[index];
    }
    argv_copy[argc] = NULL;
    execvp(program, argv_copy);
    sad();
    NOVA_LOG_ERROR("%s", strerror(errno));
    delete[] argv_copy;
}

int main(int argc, char* argv[]) {
    nova::flags::FlagValues flags(
          nova::flags::FlagMap::create_from_args(argc, argv, true));

    nova::LogOptions log_options = nova::log_options_from_flags(flags);
    nova::LogApiScope log_api_scope(log_options);

    // check if we should install and run the python guest
    if (flags.run_python_guest()) {
        NOVA_LOG_INFO("##############################################");
        NOVA_LOG_INFO("#     Starting the python agent              #");
        NOVA_LOG_INFO("##############################################");
        // sometimes this runs TOOO fast!
        // let firstboot finish its apt calls then go to town.
        boost::this_thread::sleep( boost::posix_time::seconds(5) );
        AptGuest apt(
            flags.apt_use_sudo(),
            flags.apt_self_package_name(),
            flags.apt_self_update_time_out());
        const double TIME_OUT = 90;
        apt.install("trove", TIME_OUT);
        apt.install("trove-guestagent", TIME_OUT);
        NOVA_LOG_INFO("##############################################");
        NOVA_LOG_INFO("#     python agent install complete          #");
        NOVA_LOG_INFO("##############################################");
        return 0;
    }  // end python guest start

    // run the normal sneaky-pete path here
    std::string manager = flags.datastore_manager();
    const char * program_name = 0;
    if (manager == "mysql" || manager == "percona" || manager == "maria") {
        program_name = "sneaky-pete-mysql";
    }
    else if (manager == "redis")
    {
        program_name = "sneaky-pete-redis";
    }

    if (program_name) {
        run(program_name, argc, argv);
        NOVA_LOG_ERROR("Couldn't load program \"%s\".", program_name);
    } else {
        sad();
        NOVA_LOG_ERROR("Don't know how to handle datastore %s!", manager);
    }

    return 1;
}
