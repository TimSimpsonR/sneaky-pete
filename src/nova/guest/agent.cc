#include "pch.hpp"
#include "agent.h"
#include <boost/assign/list_of.hpp>
#include "nova/process.h"

using namespace boost::assign;
using std::auto_ptr;
using nova::process::CommandList;
using boost::format;
using boost::optional;
using namespace nova;
using namespace nova::flags;
using namespace nova::guest;
using namespace nova::db::mysql;
using namespace nova::rpc;
using std::string;
using std::vector;


#define CATCH_RPC_METHOD_ERRORS


namespace nova {  namespace guest { namespace agent {

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

void run_json_method(vector<MessageHandlerPtr> & handlers,
                     const char * msg) {
    JsonObject obj(msg);
    GuestInput input;
    Receiver::init_input_with_json(input, obj);
    run_method(handlers, input);
}

GuestOutput run_method(vector<MessageHandlerPtr> & handlers, GuestInput & input) {
    GuestOutput output;

    #ifdef CATCH_RPC_METHOD_ERRORS
    try {
    #endif
        BOOST_FOREACH(MessageHandlerPtr & handler, handlers) {
            output.result = handler->handle_message(input);
            if (output.result) {
                break;
            }
        }
        if (!output.result) {
            NOVA_LOG_ERROR("No method found!")
            throw GuestException(GuestException::NO_SUCH_METHOD);
        }
        output.failure = boost::none;
    #ifdef CATCH_RPC_METHOD_ERRORS
    } catch(const std::exception & e) {
        NOVA_LOG_ERROR("Error running method %s : %s",
                   input.method_name.c_str(), e.what());
        NOVA_LOG_ERROR(e.what());
        output.result.reset();
        output.failure = e.what();
    }
    #endif

    return output;
}

void message_loop(ResilientReceiver & receiver,
                  vector<MessageHandlerPtr> & handlers) {
    while(true) {
#ifndef _DEBUG
    try {
#endif
        GuestInput input = receiver.next_message();
        NOVA_LOG_INFO("method=%s", input.method_name.c_str());

        GuestOutput output(run_method(handlers, input));

        receiver.finish_message(output);
#ifndef _DEBUG
        } catch (const std::exception & e) {
            NOVA_LOG_ERROR("std::exception error: %s", e.what());
        } catch (...) {
            NOVA_LOG_ERROR("An exception ocurred of unknown origin!");
        }
#endif
    }
}

} } } // end namespace
