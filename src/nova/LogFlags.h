#ifndef __NOVA_LOGFLAGS_H
#define __NOVA_LOGFLAGS_H

#include "nova/flags.h"
#include "nova/Log.h"
#include <boost/optional.hpp>

namespace nova {

    LogOptions log_options_from_flags(const flags::FlagValues & flags) {
        boost::optional<LogFileOptions> log_file_options;
        if (flags.log_file_path()) {
            LogFileOptions ops(flags.log_file_path().get(),
                               flags.log_file_max_size(),
                               flags.log_file_max_time(),
                               flags.log_file_max_old_files().get_value_or(30));
            log_file_options = boost::optional<LogFileOptions>(ops);
        } else {
            log_file_options = boost::none;
        }
        LogOptions log_options(log_file_options,
                               flags.log_use_std_streams(),
                               flags.log_show_trace());

        return log_options;
    }
}

#endif
