#include "pch.hpp"
#include "nova/guest/agent.h"
#include "nova/guest/guest.h"
#include "nova/rpc/sender.h"
#include "nova/flags.h"
#include "nova/Log.h"
#include <boost/tuple/tuple.hpp>


using nova::guest::agent::execute_main;
using namespace nova::flags;
using namespace nova::guest;
using namespace nova::db::mysql;
using namespace nova::rpc;
using nova::utils::ThreadBasedJobRunner;
using std::vector;
using nova::JsonData;
using nova::JsonDataPtr;
using nova::JsonObject;
using nova::JsonObjectPtr;
using nova::LogOptions;
using nova::LogFileOptions;

// Begin anonymous namespace.
namespace {

void SendMessages(ResilientSenderPtr sender) {
    std::string HELLO = "{\"oslo.message\": \"{"
                            "\\\"method\\\": \\\"TURBO\\\", "
                            "\\\"args\\\": {}}\"}";

    while(true) {
        try {
        NOVA_LOG_INFO("Sending a HELLO.");
        sender->send(HELLO.c_str());
        } catch (std::exception ex) {
            NOVA_LOG_ERROR("Exception! %s", ex.what());
            throw ex;
        }
    }
}


LogOptions log_options_from_flags(const FlagValues & flags) {
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

} // end anonymous namespace

int main(int argc, char* argv[]) {
    FlagValues flags(
        nova::flags::FlagMap::create_from_args(argc, argv, true));

    LogOptions log_options = log_options_from_flags(flags);
    nova::LogApiScope log_api_scope(log_options);


    typedef boost::shared_ptr <boost::thread> thread_ptr;
    vector<thread_ptr> threads;
    
    std::string topic = str(boost::format("guestagent.%s") % flags.guest_id());

    ResilientSenderPtr sender(
        new ResilientSender(
            flags.rabbit_host(), flags.rabbit_port(),
            flags.rabbit_userid(), flags.rabbit_password(),
            flags.rabbit_client_memory(),
            topic.c_str(), flags.control_exchange(),
            flags.rabbit_reconnect_wait_time()));

    const int worker_count = 10;
    for (int i = 0; i < worker_count; i++) {
        thread_ptr ptr(new boost::thread(SendMessages, sender));
        threads.push_back(ptr);
    }

    while(true){}

    return 0;
}
