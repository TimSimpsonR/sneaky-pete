#define BOOST_TEST_MODULE Send_and_Receive
#include <boost/test/unit_test.hpp>

#include "nova/rpc/amqp.h"
#include <boost/date_time.hpp>
#include "nova/flags.h"
#include <boost/thread.hpp>
#include "nova/rpc/receiver.h"
#include "nova/rpc/sender.h"
#include "nova/db/mysql.h"
#include <string>
#include <stdlib.h>

#define CHECK_POINT() BOOST_CHECK_EQUAL(2,2); NOVA_LOG_DEBUG2("At line %d...", __LINE__);
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

BOOST_GLOBAL_FIXTURE(GlobalFixture);

BOOST_AUTO_TEST_CASE(SendingAMessage)
{
    CHECK_POINT();
    FlagValues flags(get_flags());
    CHECK_POINT();
    AmqpConnectionPtr connection = AmqpConnection::create(
        flags.rabbit_host(), flags.rabbit_port(), flags.rabbit_userid(),
        flags.rabbit_password(), flags.rabbit_client_memory());
    CHECK_POINT();
    Receiver receiver(connection, TOPIC, "nova");
    CHECK_POINT();
    NOVA_LOG_INFO("TEST - Created receiver");
    CHECK_POINT();
    Sender sender(connection, TOPIC);
    CHECK_POINT();
    NOVA_LOG_INFO("TEST - Created sender");
    CHECK_POINT();
    const char MESSAGE_ONE [] =
    "{"
    "    'method':'list_users'"
    "}";
    CHECK_POINT();
    JsonObject input(MESSAGE_ONE);
    CHECK_POINT();
    sender.send(input);
    CHECK_POINT();
    NOVA_LOG_INFO("TEST - send obj");
    {
        GuestInput input = receiver.next_message();

        BOOST_CHECK_EQUAL(input.args->to_string(), "{ }");
        BOOST_CHECK_EQUAL("list_users", input.method_name);

        GuestOutput output;
        output.failure = boost::none;
        output.result = JsonData::from_string("ok");

        CHECK_POINT();
        receiver.finish_message(output);
        CHECK_POINT();
    }
}
