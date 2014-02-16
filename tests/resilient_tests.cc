#define BOOST_TEST_MODULE Conductor_Stress_Tests
#include <boost/test/unit_test.hpp>

#include "nova/rpc/amqp.h"
#include <boost/date_time.hpp>
#include "nova/flags.h"
#include <boost/thread.hpp>
#include "nova/rpc/receiver.h"
#include "nova/rpc/sender.h"
#include "nova/db/mysql.h"
#include <boost/format.hpp>
#include <string>
#include <stdlib.h>
#include <memory>

#define CHECK_POINT() BOOST_CHECK_EQUAL(2,2); NOVA_LOG_DEBUG("At line %d...", __LINE__);
using nova::flags::FlagMap;
using nova::flags::FlagMapPtr;
using nova::flags::FlagValues;
using nova::JsonData;
using nova::JsonObject;
using nova::JsonObjectPtr;
using namespace nova::guest;
using nova::Log;
using nova::LogApiScope;
using nova::LogOptions;
using namespace nova::rpc;
using boost::posix_time::milliseconds;
using boost::posix_time::time_duration;
using boost::thread;
using boost::format;
using std::vector;


namespace {
    const char * TOPIC = "guest";

    FlagMapPtr get_flags() {
        FlagMapPtr ptr(new FlagMap());
        char * test_args = getenv("TEST_ARGS");
        BOOST_REQUIRE_MESSAGE(test_args != 0, "TEST_ARGS environment var not defined.");
        if (test_args != 0) {
            ptr->add_from_arg(test_args);
        }
        return ptr;
    }

}

static std::string rabbitmq_host;

const char * host() {
    const char * value = getenv("AMQP_HOST");
    if (value != 0) {
        rabbitmq_host = value;
    } else {
        rabbitmq_host = "localhost";
    }
    return rabbitmq_host.c_str();
}

struct GlobalFixture {

    LogApiScope log;

    GlobalFixture()
    : log(LogOptions::simple()) {
    }

};


struct SenderWorker {
    ResilientSenderPtr sender;
    int threadid;
    int count;

    SenderWorker(ResilientSenderPtr sender, int threadid, int count)
    :
    sender(sender),
    threadid(threadid),
    count(count)
    {}

    void work() {
        const char * MESSAGE = "{\"oslo.message\": \"{"
                                   "\\\"method\\\": \\\"testing\\\", "
                                   "\\\"args\\\": {"
                                       "\\\"order\\\": %d, "
                                       "\\\"thread\\\": %d"
                                       "}}\"}";
        
        for (int i=0; i<count; i++){
            std::string msg = str(format(MESSAGE) % i % threadid);
            sender->send(msg.c_str());
        }
    }
};


BOOST_GLOBAL_FIXTURE(GlobalFixture);

BOOST_AUTO_TEST_CASE(SendingSomeMessages)
{
    CHECK_POINT();
    FlagValues flags(get_flags());

    ResilientSenderPtr sender(new ResilientSender(flags.rabbit_host(), flags.rabbit_port(),
                                                  flags.rabbit_userid(), flags.rabbit_password(),
                                                  flags.rabbit_client_memory(), TOPIC, "restests", 60));

    NOVA_LOG_INFO("TEST - Created RSender");

    CHECK_POINT();

    ResilientReceiver receiver(flags.rabbit_host(), flags.rabbit_port(), flags.rabbit_userid(), flags.rabbit_password(), flags.rabbit_client_memory(), TOPIC, "restests", 60);

    NOVA_LOG_INFO("TEST - Created RReceiver");

    CHECK_POINT();

    const int worker_count = 10;
    typedef boost::shared_ptr <boost::thread> thread_ptr;
    vector<thread_ptr> threads;
    vector<SenderWorker> workers;

    const int message_count = 1000;
    for (int t = 0; t < worker_count; t ++) {
        workers.push_back(SenderWorker(sender, t, message_count));
    }
    for (int i = 0; i < worker_count; i ++) {
        CHECK_POINT();
        thread_ptr ptr(new boost::thread(&SenderWorker::work, &workers[i]));
        threads.push_back(ptr);
    }

    CHECK_POINT();
    const int total_messages = worker_count * message_count;
    vector<int> seen;
    for (int t=0; t<worker_count; t++){
        seen.push_back(0);
    }
    for (int message=0; message<total_messages; message++){
        GuestInput input = receiver.next_message();

        BOOST_CHECK_EQUAL("testing", input.method_name);
        int t = input.args->get_int("thread");
        int o = input.args->get_int("order");
        seen[t]++;

        BOOST_CHECK_EQUAL(o, o);
        NOVA_LOG_INFO("Saw thread %i, order %i.", t, o);

        GuestOutput output;
        output.failure = boost::none;
        output.result = JsonData::from_string("ok");

        receiver.finish_message(output);
    }

    for (int t=0; t<worker_count; t++) {
        NOVA_LOG_INFO("Seen count for %i: %i", t, seen[t]);
        BOOST_CHECK_EQUAL(seen[t], message_count);
    }
    
}
