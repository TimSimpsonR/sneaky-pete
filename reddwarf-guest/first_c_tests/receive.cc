#include "/usr/local/include/AMQPcpp.h"
#include <unistd.h>
#include <syslog.h>
#include "mysql_connection.h"

#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>

int main() {
    try {
        daemon(1,0);
        AMQP amqp("guest:guest@localhost:5672/");
        
        sql::Driver *driver;
        sql::Connection *con;
        sql::Statement *stmt;
        sql::ResultSet *res;
        driver = get_driver_instance();
        con = driver->connect("tcp://127.0.0.1:3306", "root", "");
        // Connect to the MySQL test database 
        con->setSchema("information_schema");
        
        AMQPQueue * temp_queue = amqp.createQueue("guest.hostname");
        //Assume the queue is already declared.
        // temp_queue->Declare();
        while(true) {
            sleep(2);
            temp_queue->Get();
            
            AMQPMessage * m= temp_queue->getMessage();
            if (m->getMessageCount() > -1) {
                int original_message_count = m->getMessageCount();
                for (int i = 0; i <= original_message_count; i++) {
                    syslog(LOG_INFO, "message %s, key %s, tag %i, ex %s, c-type %s, c-enc %s" ,
                                      m->getMessage(), m->getRoutingKey().c_str(), m->getDeliveryTag(), m->getExchange().c_str(), 
                                      m->getHeader("Content-type").c_str(), m->getHeader("Content-encoding").c_str());
                    temp_queue->Ack(m->getDeliveryTag());
                    temp_queue->Get();
                    
                    stmt = con->createStatement();
                    res = stmt->executeQuery("select * from TABLES;");
                    while (res->next()) {
                        syslog(LOG_INFO, "\t... MySQL replies: %s", res->getString("TABLE_NAME").c_str());
                    }
                    res->close();
                    stmt->close();
                    delete res;
                    delete stmt;
                }
            }
        }
        
        delete con;
                
    } catch (AMQPException e) {
        cout << e.getMessage() << endl;
        syslog(LOG_INFO,"exception is %s", e.getMessage().c_str());
    }
    return 0;
}
