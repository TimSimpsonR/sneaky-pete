#include <AMQPcpp.h>
#include <sstream>
#include <unistd.h>

int main(int argc, const char* argv[]) {
	if (argc < 2) {
		std::cerr << "Please specify the method name as the first argument."
		          << std::endl;
		return 1;
	}
	const char * method_name = argv[1];
    try {
        // daemon(1,0);
        AMQP amqp("guest:guest@localhost:5672/");
        const string exchange("guest.hostname_exchange");
        AMQPExchange * ex = amqp.createExchange(exchange);
        ex->Declare(exchange, "direct");

        //Make sure the exchange was bound to the queue to actually send the messages thru
        AMQPQueue * q = amqp.createQueue("guest.hostname");
        q->Declare();
        q->Bind(exchange, "");

        ex->setHeader("Delivery-mode", 2);
        ex->setHeader("Content-type", "text/text");
        ex->setHeader("Content-encoding", "UTF-8");
        std::stringstream publish_string;
        publish_string << "{'method': '" << method_name << "}";
        cout << publish_string.str() <<endl;
        ex->Publish(publish_string.str().c_str(), "");

    } catch (AMQPException e) {
        std::cout << "An exception occured (code " << e.getReplyCode()
                  << "):" << e.getMessage() << std::endl;
    }
    return 0;
}
