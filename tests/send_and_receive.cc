#define BOOST_TEST_MODULE Send_and_Receive
#include <boost/test/unit_test.hpp>

#include <boost/date_time.hpp>
#include <boost/thread.hpp>
#include "receiver.h"
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
        Receiver receiver("guest:guest@localhost:5672/", "guest.hostname", "%");
        /*MySqlGuestPtr guest(new MySqlGuest());
        MySqlMessageHandler handler(guest);

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


BOOST_AUTO_TEST_CASE(TestFail)
{
	char * mem = new char[20];
	mem[0] = 'x';
	int x(255);
	int y(255);
	BOOST_CHECK_EQUAL(x, 255);
	BOOST_CHECK_EQUAL(y, 255);

	BOOST_CHECK_EQUAL(x, 255);
	BOOST_CHECK_EQUAL(y, 255);
	delete[] mem;
}
