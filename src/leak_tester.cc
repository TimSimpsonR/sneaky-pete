#include "amqp_helpers.h"
#include "guest.h"
#include "sql_guest.h"
#include "receiver.h"
#include "configfile.h"
#include "log.h"
#include <json/json.h>
#include "log.h"
#include <sstream>
#include "configfile.h"
#include "sender.h"


int main(int argc, const char* argv[]) {
    Log log;

    Configfile::Configfile configfile("config/test-configfile.txt");

    std::string amqp_host = configfile.get_string("amqp_host");
    int amqp_port = configfile.get_int("amqp_port");
    std::string amqp_user_name = configfile.get_string("amqp_user_name");
    std::string amqp_password = configfile.get_string("amqp_password");
    std::string amqp_queue = configfile.get_string("amqp_queue");

    Receiver receiver(amqp_host.c_str(), amqp_port, amqp_user_name.c_str(),
                      amqp_password.c_str(),
                      amqp_queue.c_str());
    //Receiver receiver("localhost", 5672, "guest", "guest", "guest.hostname");
    MySqlGuestPtr sql(new MySqlGuest("unix:///var/run/mysqld/mysqld.sock"));
    MySqlMessageHandler handler(sql);

    for (int i = 0; i < 12; i ++) {
        Sender sender("localhost", 5672, "guest", "guest",
                      "guest.hostname_exchange", "guest.hostname", "");
        sender.send("{'method':'is_root_enabled'}");

       json_object * input = receiver.next_message();
        json_object * output = handler.handle_message(input);
        //json_object * output = json_tokener_parse("{'good'}");
        std::stringstream msg;
        msg << i << ". ";
        msg << "" << json_object_to_json_string(input);
        msg << "" << json_object_to_json_string(output);
        log.info(msg.str());

        receiver.finish_message(input, output);
    }
    /*
    AmqpConnectionPtr connection = AmqpConnection::create("localhost", 5672,
                                                          "guest", "guest");
    AmqpChannelPtr queue = connection->new_channel();
    AmqpQueueMessagePtr msg;
    msg = queue->get_message("guest.hostname");*/

    return 0;
}
