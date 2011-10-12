#define BOOST_TEST_MODULE Send_and_Receive
#include <boost/test/unit_test.hpp>

#include <boost/date_time.hpp>
#include <boost/thread.hpp>
#include "nova/rpc/receiver.h"
#include "nova/rpc/sender.h"
#include "nova/guest/mysql.h"

using nova::Log;
using nova::JsonObject;
using nova::JsonObjectPtr;
using namespace nova::guest;
using namespace nova::rpc;
using boost::posix_time::milliseconds;
using boost::posix_time::time_duration;
using boost::thread;


struct Daemon {

    bool quit;

    Daemon()
    : quit(false)
    {
    }

    void operator()() {
        char * f = new char[500];
        delete [] f;
        //Receiver receiver("guest:guest@localhost:5672/", "guest.hostname", "%");


        //MySqlGuestPtr guest(new MySqlGuest());
        //MySqlMessageHandler handler(guest);
        /*
        while(!quit) {
            syslog(LOG_INFO, "getting and getting");
            json_object * input = receiver.next_message();
            syslog(LOG_INFO, "output of json %s",
                   json_object_to_json_string(input));
            json_object * output = handler.handle_message(input);
            receiver.finish_message(input, output);
        }*/
    }

};
#ifdef NOT_NOW

BOOST_AUTO_TEST_CASE(Flood)
{
    Daemon daemon;
    daemon.quit = false;

    thread daemon_thread(daemon);

    //TODO

    daemon.quit = true;
    time_duration timeout = milliseconds(500);
    daemon_thread.timed_join(timeout);
}

#endif

/** Making sure we can construct and destruct these types without leaking. */
BOOST_AUTO_TEST_CASE(ConstructingAndDestructingASender) {
    Sender sender("localhost", 5672, "guest", "guest", "guest.hostname_exchange",
                  "guest.hostname", "");
}


BOOST_AUTO_TEST_CASE(ConstructingAndDestructingAReceiver) {
    Receiver receiver("localhost", 5672, "guest", "guest",
                      "guest.hostname");
}


BOOST_AUTO_TEST_CASE(SendingANormalMessage_NEW)
{
    Log log;

    Receiver receiver("localhost", 5672, "guest", "guest", "guest.hostname");
    log.info("TEST - Created receiver");
    Sender sender("localhost", 5672, "guest", "guest", "guest.hostname_exchange",
                  "guest.hostname", "");

    log.info("TEST - Created sender");
    const char MESSAGE_ONE [] =
    "{"
    "    'method':'list_users'"
    "}";
    JsonObjectPtr send_object(new JsonObject(MESSAGE_ONE));
    sender.send(send_object);
    log.info("TEST - send obj");
    {
        JsonObjectPtr obj = receiver.next_message();
        BOOST_CHECK_MESSAGE(!!obj, "Must receive an object.");

        std::string method;
        obj->get_string("method", method);
        BOOST_CHECK_EQUAL(method, "list_users");

        const char RESPONSE [] =
        "{"
        "    'status':'ok'"
        "}";
        JsonObjectPtr rtn_obj(new JsonObject(RESPONSE));
        receiver.finish_message(obj, rtn_obj);
    }
}

