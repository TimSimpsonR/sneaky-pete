#define BOOST_TEST_MODULE Send_and_Receive
#include <boost/test/unit_test.hpp>

#include "nova/rpc/amqp.h"
#include <boost/date_time.hpp>
#include <boost/thread.hpp>
#include "nova/rpc/receiver.h"
#include "nova/rpc/sender.h"
#include "nova/guest/mysql.h"
#include <string>
#include <stdlib.h>

#define CHECK_POINT() BOOST_CHECK_EQUAL(2,2); log.debug("At line %d...", __LINE__);
using nova::Log;
using nova::JsonObject;
using nova::JsonObjectPtr;
using namespace nova::guest;
using namespace nova::rpc;
using boost::posix_time::milliseconds;
using boost::posix_time::time_duration;
using boost::thread;


namespace {
    const char * HOST = "10.0.4.15";
    const int PORT = 5672;
    const char * USER_NAME = "guest";
    const char * PASSWORD = "guest";
    const char * TOPIC = "test-topic";
    const size_t CLIENT_MEMORY = 1024 * 4;
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


BOOST_AUTO_TEST_CASE(SendingAMessage)
{
    Log log;
    CHECK_POINT();
    AmqpConnectionPtr connection = AmqpConnection::create(
        HOST, PORT, USER_NAME, PASSWORD, CLIENT_MEMORY);
    CHECK_POINT();
    Receiver receiver(connection, TOPIC);
    CHECK_POINT();
    log.info("TEST - Created receiver");
    CHECK_POINT();
    Sender sender(connection, TOPIC);
    CHECK_POINT();
    log.info("TEST - Created sender");
    CHECK_POINT();
    const char MESSAGE_ONE [] =
    "{"
    "    'method':'list_users'"
    "}";
    CHECK_POINT();
    JsonObject send_object(MESSAGE_ONE);
    CHECK_POINT();
    sender.send(send_object);
    CHECK_POINT();
    log.info("TEST - send obj");
    {
        JsonObjectPtr obj = receiver.next_message();
        CHECK_POINT();
        BOOST_CHECK_MESSAGE(!!obj, "Must receive an object.");

        std::string method;
        obj->get_string("method", method);
        CHECK_POINT();
        BOOST_CHECK_EQUAL(method, "list_users");

        const char RESPONSE [] =
        "{"
        "    'status':'ok'"
        "}";
        JsonObjectPtr rtn_obj(new JsonObject(RESPONSE));
        CHECK_POINT();
        receiver.finish_message(obj, rtn_obj);
        CHECK_POINT();
    }
}
