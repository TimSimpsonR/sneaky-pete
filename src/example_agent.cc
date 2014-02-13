#include "pch.hpp"
#include "nova/guest/agent.h"
#include "nova/rpc/sender.h"
#include <boost/tuple/tuple.hpp>


using nova::guest::agent::execute_main;
using namespace nova::flags;
using namespace nova::guest;
using nova::utils::ThreadBasedJobRunner;
using nova::rpc::ResilientSenderPtr;
using std::vector;

// Begin anonymous namespace.
namespace {

struct EmptyAppUpdate
{
    void operator() ()
    {
        // Update status by calling nova_db.
    }
};

typedef boost::shared_ptr<EmptyAppUpdate> EmptyAppUpdatePtr;


struct Func {
    boost::tuple<vector<MessageHandlerPtr>, EmptyAppUpdatePtr>
        operator() (const FlagValues & flags,
                    ResilientSenderPtr & sender,
                    ThreadBasedJobRunner & job_runner)
    {
        /* Create JSON message handlers. */
        vector<MessageHandlerPtr> handlers;

        EmptyAppUpdatePtr updater(new EmptyAppUpdate());

        return boost::make_tuple(handlers, updater);
    }

};

} // end anonymous namespace


int main(int argc, char* argv[]) {
    return execute_main<Func, EmptyAppUpdatePtr>("Example Edition", argc, argv);
}
