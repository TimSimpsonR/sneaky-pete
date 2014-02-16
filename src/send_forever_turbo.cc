#include "pch.hpp"
#include "nova/guest/agent.h"
#include "nova/guest/guest.h"
#include "nova/rpc/sender.h"
#include "nova/flags.h"
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

// Begin anonymous namespace.
namespace {

static bool quit;


class ListenForQuit: public MessageHandler
{
    JsonDataPtr handle_message(const GuestInput & input)
    {
        std::string m(input.method_name);
        if (m == "quit") {
            quit = true;
        }
        return JsonData::from_null();
    }    
};


struct EmptyAppUpdate {
    void operator() () {
    }
};

typedef boost::shared_ptr<EmptyAppUpdate> EmptyAppUpdatePtr;


void SendMessages(ResilientSenderPtr sender) {
    std::string HELLO = "{\"oslo.message\": \"{"
                            "\\\"method\\\": \\\"hello\\\", "
                            "\\\"args\\\": {}}\"}";

    while(!quit) {
        sender->send(HELLO.c_str());
    }
    NOVA_LOG_INFO("I am quitting.")
}


struct Func {

    typedef boost::shared_ptr <boost::thread> thread_ptr;
    vector<thread_ptr> threads;

    boost::tuple<vector<MessageHandlerPtr>, EmptyAppUpdatePtr>
        operator() (const FlagValues & flags,
                    MySqlConnectionWithDefaultDbPtr & nova_db,
                    ThreadBasedJobRunner & job_runner)

    {
        quit = false;
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

        vector<MessageHandlerPtr> handlers;
        MessageHandlerPtr chill(new ListenForQuit());
        handlers.push_back(chill);

        EmptyAppUpdatePtr updater(new EmptyAppUpdate());

        return boost::make_tuple(handlers, updater);
    }

};

} // end anonymous namespace


int main(int argc, char* argv[]) {
    return execute_main<Func, EmptyAppUpdatePtr>("Send Forever Turbo", argc, argv);
}
