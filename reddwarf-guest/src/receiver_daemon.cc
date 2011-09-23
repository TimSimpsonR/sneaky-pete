#include "guest.h"
#include "sql_guest.h"
#include "receiver.h"
#include "configfile.h"
#include "log.h"
#include <json/json.h>
#include <sstream>

const char error_message [] =
"{"
"    \"error\":\"could not interpret message.\""
"}";

int main(int argc, const char* argv[]) {
    Log log;
    std::string config_location = "config/test-configfile.txt";
    if (argc >= 2) {
        config_location = argv[1];
    }

    Configfile::Configfile configfile(config_location);

    std::string amqp_host = configfile.get_string("amqp_host");
    int amqp_port = configfile.get_int("amqp_port");
    std::string amqp_user_name = configfile.get_string("amqp_user_name");
    std::string amqp_password = configfile.get_string("amqp_password");
    std::string amqp_queue = configfile.get_string("amqp_queue");

    Receiver receiver(amqp_host.c_str(), amqp_port, amqp_user_name.c_str(), amqp_password.c_str(),
                      amqp_queue.c_str());

    std::string mysql_uri = configfile.get_string("mysql_uri");
    MySqlGuestPtr guest(new MySqlGuest(mysql_uri));

    MySqlMessageHandler handler(guest);

#ifndef _DEBUG
    try {
        daemon(1,0);
#endif

        while(true) {
            log.info("getting and getting");
            json_object * input = receiver.next_message();
            log.info2("output of json %s", json_object_to_json_string(input));
            json_object * output = 0;
            #ifndef _DEBUG
            try {
            #endif
                output = handler.handle_message(input);
            #ifndef _DEBUG
            } catch(sql::SQLException & e) {
                log.info2("receiver exception is %s %i %s", e.what(),
                            e.getErrorCode(), e.getSQLState().c_str());
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
