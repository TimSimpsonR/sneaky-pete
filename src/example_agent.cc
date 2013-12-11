#include "pch.hpp"
#include "nova/guest/agent.h"
#include "nova/guest/guest.h"
#include <boost/tuple/tuple.hpp>


using nova::guest::agent::execute_main;
using namespace nova::flags;
using namespace nova::guest;
using namespace nova::db::mysql;
using nova::utils::ThreadBasedJobRunner;
using std::vector;

// Begin anonymous namespace.
namespace {

struct EmptyAppUpdate
{
    void update()
    {
        // Update status by calling nova_db.
    }
};

typedef boost::shared_ptr<EmptyAppUpdate> EmptyAppUpdatePtr;


struct Func {
    boost::tuple<vector<MessageHandlerPtr>, EmptyAppUpdatePtr>
        operator() (const FlagValues & flags,
                    MySqlConnectionWithDefaultDbPtr & nova_db,
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
