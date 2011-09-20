#define BOOST_TEST_MODULE Send_and_Receive
#include <boost/test/unit_test.hpp>

#include <boost/date_time.hpp>
#include <boost/thread.hpp>
#include "receiver.h"
#include "sender.h"
#include "sql_guest.h"

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


BOOST_AUTO_TEST_CASE(AMQPleaker) {
    /** Tests a leak in AMQPCpp. */

    AMQPExchange * ex = 0;
    AMQP amqp("guest:guest@localhost:5672/");
    const std::string exchange_name = "guest.hostname_exchange";

    AMQPQueue *queue = amqp.createQueue("guest.hostname");
    queue->Declare();

    ex = amqp.createExchange(exchange_name);
    ex->Declare(exchange_name, "direct");

    queue->Bind(exchange_name, "");
}

/*BOOST_AUTO_TEST_CASE(DefiningASender) {
    Sender sender("guest:guest@localhost:5672/", "guest.hostname_exchange",
                  "guest.hostname");
}*/

#ifdef FDJGKJGDFLJ
BOOST_AUTO_TEST_CASE(SendingANormalMessage)
{
    Sender sender("guest:guest@localhost:5672/", "guest.hostname_exchange",
                  "guest.hostname");/*
    Receiver receiver("guest:guest@localhost:5672/", "guest.hostname", "%");
*/
    const char MESSAGE_ONE [] =
    "{"
    "    'method':'list_users'"
    "}";
    json_object * send_object = json_object_new_string(MESSAGE_ONE);
  //  sender.send(send_object);
    {
        /*
        json_object * obj = receiver.next_message();
        BOOST_REQUIRE_MESSAGE((bool)json_object_is_type(obj, json_type_object),
                              "Must receive an object.");
        json_object * method_obj = json_object_object_get(obj, "method");
        BOOST_REQUIRE_NE(method_obj, (json_object *) 0);
        const char * method_str = json_object_to_json_string(method_obj);
        BOOST_REQUIRE_EQUAL(method_str, "list_users");
        const char RESPONSE [] =
        "{"
        "    'status':'ok'"
        "}";
        json_object * rtn_obj = json_object_new_string(RESPONSE);
        receiver.finish_message(obj, rtn_obj);*/
    }

    json_object_put(send_object);

	// FROG
    //    sql::Driver * driver = get_driver_instance();

      //  sql::Connection *con = driver->connect("tcp://127.0.0.1:3306", "root", "");
        //delete con;
        //delete driver;
        // FROG
}
#endif
