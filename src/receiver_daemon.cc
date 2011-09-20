#include "guest.h"
#include "sql_guest.h"
#include "receiver.h"
#include <AMQPcpp.h>
#include <boost/foreach.hpp>
#include <json/json.h>
#include <sstream>

const char error_message [] =
"{"
"    \"error\":\"could not interpret message.\""
"}";

int main() {
    Receiver receiver("guest:guest@localhost:5672/", "guest.hostname", "%");
    MySqlGuestPtr guest(new MySqlGuest());
    MySqlMessageHandler handler(guest);

#ifndef _DEBUG
    try {
        daemon(1,0);
#endif
        while(true) {
            syslog(LOG_INFO, "getting and getting");
            json_object * input = receiver.next_message();
            syslog(LOG_INFO, "output of json %s",
                   json_object_to_json_string(input));
            json_object * output = 0;
            #ifndef _DEBUG
            try {
            #endif
                output = handler.handle_message(input);
            #ifndef _DEBUG
            } catch(sql::SQLException & e) {
                syslog(LOG_ERR, "receiver exception is %s %i %s", e.what(),
                       e.getErrorCode(), e.getSQLState().c_str());
                output = json_object_new_string(error_message);
            }
            #endif
            receiver.finish_message(input, output);
        }
#ifndef _DEBUG
    } catch (AMQPException e) {
        syslog(LOG_ERR,"Exception! Code %i, message = %s",
               e.getReplyCode(),
               e.getMessage().c_str());
    }
#endif
    return 0;
}
