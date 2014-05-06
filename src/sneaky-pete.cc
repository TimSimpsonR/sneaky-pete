#include "nova/flags.h"
#include "nova/Log.h"
#include "nova/LogFlags.h"


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

    std::string manager = flags.datastore_manager();
    const char * program_name = 0;
    if (manager == "mysql" || manager == "percona") {
        program_name = "sneaky-pete-mysql";
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
