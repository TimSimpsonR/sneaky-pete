#include "guest.h"
#include <AMQPcpp.h>
#include <boost/foreach.hpp>
#include <json/json.h>
#include <sstream>

int main() {
    const string default_host = "%";

    try {
        daemon(1,0);
        AMQP amqp("guest:guest@localhost:5672/");
        Guest::Guest *g= new Guest::Guest();

        AMQPQueue * temp_queue = amqp.createQueue("guest.hostname");
        //Assume the queue is already declared.
        temp_queue->Declare();

        // g->create_database("testing", "utf8", "utf8_general_ci");
        while(true) {
            sleep(2);
            temp_queue->Get();
            syslog(LOG_INFO, "getting and getting");

            AMQPMessage * m= temp_queue->getMessage();
            if (m->getMessageCount() > -1) {
                int original_message_count = m->getMessageCount();
                for (int i = 0; i <= original_message_count; i++) {
                    syslog(LOG_INFO, "message %s, key %s, tag %i, ex %s, c-type %s, c-enc %s" ,
                                      m->getMessage(), m->getRoutingKey().c_str(), m->getDeliveryTag(), m->getExchange().c_str(),
                                      m->getHeader("Content-type").c_str(), m->getHeader("Content-encoding").c_str());

                    json_object *new_obj = json_tokener_parse(m->getMessage());
                    syslog(LOG_INFO, "output of json %s", json_object_to_json_string(new_obj));
                    json_object *method = json_object_object_get(new_obj, "method");
                    string method_name = json_object_to_json_string(method);

                    if (method_name == "\"create_user\"") {
                        try {
                            string guest_return = g->create_user("username", "password", default_host);
                            syslog(LOG_INFO, "guest call %s", guest_return.c_str());
                        } catch (sql::SQLException &e) {
                            syslog(LOG_ERR,"receiver exception is %s %i %s", e.what(), e.getErrorCode(), e.getSQLState().c_str());
                        }
                    } else if (method_name == "\"list_users\"") {
                        try {
                            std::stringstream user_xml;
                            MySQLUserListPtr users = g->list_users();
                            user_xml << "[";
                            BOOST_FOREACH(MySQLUserPtr & user, *users) {
                                user_xml << "{'name':'" << user->get_name()
                                         << "'},";
                            }
                            user_xml << "]";
                            syslog(LOG_INFO, "guest call %s", user_xml.str().c_str());
                        } catch (sql::SQLException &e) {
                            syslog(LOG_ERR, "receiver exception is %s %i %s", e.what(), e.getErrorCode(), e.getSQLState().c_str());
                        }
                    } else if (method_name == "\"delete_user\"") {
                        try {
                            string guest_return = g->delete_user("username");
                            syslog(LOG_INFO, "guest call %s", guest_return.c_str());
                        } catch (sql::SQLException &e) {
                            syslog(LOG_ERR, "receiver exception is %s %i %s", e.what(), e.getErrorCode(), e.getSQLState().c_str());
                        }
                    } else if (method_name == "\"list_databases\"") {
                        try {
                            string guest_return = g->list_databases();
                            syslog(LOG_INFO, "guest call %s", guest_return.c_str());
                        } catch (sql::SQLException &e) {
                            syslog(LOG_ERR, "receiver exception is %s %i %s", e.what(), e.getErrorCode(), e.getSQLState().c_str());
                        }
                    } else if (method_name == "\"delete_database\"") {
                        try {
                            string guest_return = g->delete_database("database_name");
                            syslog(LOG_INFO, "guest call %s", guest_return.c_str());
                        } catch (sql::SQLException &e) {
                            syslog(LOG_ERR, "receiver exception is %s %i %s", e.what(), e.getErrorCode(), e.getSQLState().c_str());
                        }
                    } else if (method_name == "\"enable_root\"") {
                        try {
                            string guest_return = g->enable_root();
                            syslog(LOG_INFO, "roots new password %s", guest_return.c_str());
                        } catch (sql::SQLException &e) {
                            syslog(LOG_ERR, "receiver exception is %s %i %s", e.what(), e.getErrorCode(), e.getSQLState().c_str());
                        }
                    } else if (method_name == "\"disable_root\"") {
                        try {
                            string guest_return = g->disable_root();
                            syslog(LOG_INFO, "guest call %s", guest_return.c_str());
                        } catch (sql::SQLException &e) {
                            syslog(LOG_ERR, "receiver exception is %s %i %s", e.what(), e.getErrorCode(), e.getSQLState().c_str());
                        }
                    } else if (method_name == "\"is_root_enabled\"") {
                        try {
                            bool enabled = g->is_root_enabled();
                            syslog(LOG_INFO, "guest call %i", enabled);
                        } catch (sql::SQLException &e) {
                            syslog(LOG_ERR, "receiver exception is %s %i %s", e.what(), e.getErrorCode(), e.getSQLState().c_str());
                        }
                    } else {
                        syslog(LOG_ERR, "Should not happen");
                    }
                    json_object_put(new_obj);

                    temp_queue->Ack(m->getDeliveryTag());
                    temp_queue->Get();

                }
            }
        }
    } catch (AMQPException e) {
        syslog(LOG_ERR,"Exception! Code %i, message = %s",
               e.getReplyCode(),
               e.getMessage().c_str());
    }
    return 0;
}
