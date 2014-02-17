#include "pch.hpp"
#include "nova/guest/agent.h"
#include "nova/guest/guest.h"
#include "nova/flags.h"
#include "nova/rpc/sender.h"
#include <boost/tuple/tuple.hpp>


using nova::guest::agent::execute_main;
using namespace nova::flags;
using namespace nova::guest;
using namespace nova::rpc;
using nova::utils::ThreadBasedJobRunner;
using std::vector;
using nova::JsonData;
using nova::JsonDataPtr;
using nova::JsonObject;
using nova::JsonObjectPtr;

// Begin anonymous namespace.
namespace {

class IgnoreEverything: public MessageHandler
{
    JsonDataPtr handle_message(const GuestInput & input)
    { return JsonData::from_null(); }
};


struct EmptyAppUpdate { void operator ()() { } };

typedef boost::shared_ptr<EmptyAppUpdate> EmptyAppUpdatePtr;


struct Func {

    typedef boost::shared_ptr <boost::thread> thread_ptr;
    vector<thread_ptr> threads;

    boost::tuple<vector<MessageHandlerPtr>, EmptyAppUpdatePtr>
        operator() (const FlagValues & flags,
                    ResilientSenderPtr sender,
                    ThreadBasedJobRunner & job_runner)

    {
        vector<MessageHandlerPtr> handlers;
        MessageHandlerPtr chill(new IgnoreEverything());
        handlers.push_back(chill);

        EmptyAppUpdatePtr updater(new EmptyAppUpdate());

        return boost::make_tuple(handlers, updater);
    }

};

} // end anonymous namespace


int main(int argc, char* argv[]) {
    return execute_main<Func, EmptyAppUpdatePtr>("SF Turbo RECV", argc, argv);
}
