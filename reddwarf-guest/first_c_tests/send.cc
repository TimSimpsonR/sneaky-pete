#include "/usr/local/include/AMQPcpp.h"
#include <unistd.h>

int main() {
    try {
        daemon(1,0);
        AMQP amqp("guest:guest@localhost:5672/");
        string exchange = "guest.hostname_exchange";
        AMQPExchange * ex = amqp.createExchange(exchange);
        ex->Declare(exchange, "direct");
        
        //Make sure the exchange was bound to the queue to actually send the messages thru
        AMQPQueue * q = amqp.createQueue("guest.hostname");
        q->Declare();
        q->Bind(exchange, "");
                
        ex->setHeader("Delivery-mode", 2);
        ex->setHeader("Content-type", "text/text");
        ex->setHeader("Content-encoding", "UTF-8");
        
        while(true) {
            sleep(1);
            ex->Publish("THIS IS A TEST", "");
        }
        
        
    } catch (AMQPException e) {
        std::cout << e.getMessage() << std::endl;
    }
    return 0;
}
