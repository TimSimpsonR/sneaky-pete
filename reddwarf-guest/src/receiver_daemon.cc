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

using nova::guest::apt::AptGuest;
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

#ifndef _DEBUG

    try {
        daemon(1,0);
#endif

        /* Grab flag values. */
        FlagValues flags(FlagMap::create_from_args(argc, argv, true));

        /* Create Apt Guest */
        AptGuest apt_guest(flags.apt_use_sudo());

        /* Create MySQL Guest. */
        std::string mysql_uri = "localhost"; //unix:///var/run/mysqld/mysqld.sock";
        MySqlConnectionPtr sql_connection(new MySqlConnection(mysql_uri));
        MySqlGuestPtr mysql_guest(new MySqlGuest(sql_connection));

        string rabbit_queue = "guest.";
        rabbit_queue += nova::guest::utils::get_host_name();

        /* Create JSON message handlers. */
        const int handler_count = 2;
        MessageHandlerPtr handlers[handler_count];
        handlers[0].reset(new MySqlMessageHandler(mysql_guest, &apt_guest));
        handlers[1].reset(new apt::AptMessageHandler(&apt_guest));

        /* Create receiver. */
        Receiver receiver(flags.rabbit_host(), flags.rabbit_port(),
                          flags.rabbit_userid(), flags.rabbit_password(),
                          rabbit_queue.c_str(), flags.rabbit_client_memory());

        bool quit = false;
        while(!quit) {
            log.info("Waiting for next message...");
            JsonObjectPtr input = receiver.next_message();
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
                    result = handlers[i]->handle_message(input);
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
