#include "nova/guest/apt.h"
#include "nova/flags.h"
#include "nova/guest/guest.h"
#include "nova/guest/GuestException.h"
#include <boost/lexical_cast.hpp>
#include "nova/guest/mysql.h"
#include <boost/optional.hpp>
#include "nova/rpc/receiver.h"
#include "nova/ConfigFile.h"
#include <json/json.h>
#include "nova/Log.h"
#include <sstream>
#include "nova/guest/utils.h"

using boost::optional;
using namespace nova;
using namespace nova::flags;
using namespace nova::guest;
using namespace nova::guest::mysql;
using namespace nova::rpc;
using std::string;


const char error_message [] =
"{"
"    \"error\":\"could not interpret message.\""
"}";

int main(int argc, char* argv[]) {
    Log log;
    const char * config_location = "config/test-configfile.txt";
    if (argc >= 2) {
        config_location = argv[1];
    }

    ConfigFile configfile(config_location);

    /* Grab flag values. */
    FlagValuesPtr flags = FlagValues::create_from_args(argc, argv, true);

    std::string amqp_host = flags->get("rabbit_host"); //configfile.get_string("amqp_host");
    int amqp_port = flags->get_as_int("rabbit_port"); // configfile.get_as_int("amqp_port");
    string amqp_user_name = flags->get("rabbit_userid"); // configfile.get_string("amqp_user_name");
    string amqp_password = flags->get("rabbit_password"); // configfile.get_string("amqp_password");
    string amqp_queue = ::nova::guest::utils::get_host_name(); //configfile.get_string("amqp_queue");

    string host, user, password;
    optional<string> database;
    flags->get_sql_connection(host, user, password, database);

    /* Create MySQL Guest. */
    std::string mysql_uri = configfile.get_string("mysql_uri");
    MySqlGuestPtr guest(new MySqlGuest(mysql_uri));

    /* Create JSON message handlers. */
    const int handler_count = 2;
    MessageHandlerPtr handlers[handler_count];
    handlers[0].reset(new MySqlMessageHandler(guest));
    handlers[1].reset(new apt::AptMessageHandler());

    /* Create receiver. */
    Receiver receiver(amqp_host.c_str(), amqp_port, amqp_user_name.c_str(),
                      amqp_password.c_str(),
                      amqp_queue.c_str());


#ifndef _DEBUG
    try {
        daemon(1,0);
#endif
        bool quit = false;
        while(!quit) {
            log.info("getting and getting");
            JsonObjectPtr input = receiver.next_message();
            log.info2("incoming json: %s", input->to_string());
            JsonObjectPtr output;
            try {

            #ifdef _DEBUG
                std::string method_str;
                input->get_string("method", method_str);
                log.info2("method=%s", method_str.c_str());
                if (method_str == "exit") {
                    quit = true;
                }
            #endif

                JsonDataPtr result;
                for (int i = 0; i < handler_count && !result; i ++) {
                    result = handlers[0]->handle_message(input);
                }
                if (!result) {
                    throw GuestException(GuestException::NO_SUCH_METHOD);
                }
                std::stringstream msg;
                msg << "{'result':" << result->to_string() << ","
                       " 'failure':null }";
                output.reset(new JsonObject(msg.str().c_str()));
            } catch(const std::exception & e) {
                log.info2("receiver exception is %s", e.what());
                std::stringstream msg;
                msg << "{'failure':{'exc_type':'std::exception',"
                       "'value':'" << e.what() << "', "
                       "'traceback':'unavailable' } }";
                output.reset(new JsonObject(msg.str().c_str()));
            }
            receiver.finish_message(input, output);
        }
#ifndef _DEBUG
    } catch (const std::exception & e) {
        log.error2("Error: %s", e.what());
    }
#endif
    return 0;
}
