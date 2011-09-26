#include "nova/guest/guest.h"
#include "nova/guest/sql_guest.h"
#include "nova/rpc/receiver.h"
#include "nova/configfile.h"
#include <json/json.h>
#include "nova/log.h"
#include <sstream>


using namespace nova;
using namespace nova::guest;
using namespace nova::rpc;


const char error_message [] =
"{"
"    \"error\":\"could not interpret message.\""
"}";

int main(int argc, const char* argv[]) {
    Log log;
    const char * config_location = "config/test-configfile.txt";
    if (argc >= 2) {
        config_location = argv[1];
    }

    Configfile::Configfile configfile(config_location);

    std::string amqp_host = configfile.get_string("amqp_host");
    int amqp_port = configfile.get_int("amqp_port");
    std::string amqp_user_name = configfile.get_string("amqp_user_name");
    std::string amqp_password = configfile.get_string("amqp_password");
    std::string amqp_queue = configfile.get_string("amqp_queue");

    Receiver receiver(amqp_host.c_str(), amqp_port, amqp_user_name.c_str(),
                      amqp_password.c_str(),
                      amqp_queue.c_str());

    std::string mysql_uri = configfile.get_string("mysql_uri");
    MySqlGuestPtr guest(new MySqlGuest(mysql_uri));

    MySqlMessageHandler handler(guest);

#ifndef _DEBUG
    try {
        daemon(1,0);
#endif
        bool quit = false;
        while(!quit) {
            log.info("getting and getting");
            json_object * input = receiver.next_message();
            log.info2("output of json %s", json_object_get_string(input));
            json_object * output = 0;
            #ifndef _DEBUG
            try {
            #endif

            #ifdef _DEBUG
                json_object * method_obj = json_object_object_get(input, "method");
                std::string method_str(json_object_get_string(method_obj));
                log.info2("method=%s", method_str.c_str());
                if (method_str == "exit") {
                    quit = true;
                }
            #endif

                output = handler.handle_message(input);
            #ifndef _DEBUG
            } catch(sql::SQLException & e) {
                log.info2("receiver exception is %s %i %s", e.what(),
                            e.getErrorCode(), e.getSQLState().c_str());
                output = json_tokener_parse(error_message);
            }
            #endif
            receiver.finish_message(input, output);
        }
#ifndef _DEBUG
    } catch (const std::exception & e) {
        log.error2("Error: %s", e.what());
    }
#endif
    return 0;
}
