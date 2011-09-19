#include <AMQPcpp.h>
#include <sstream>
#include <unistd.h>

//TODO: Move this class into its own .h / .cc pair.

class Sender {

    public:
        Sender(const char * host_name, const char * exchange_name,
               const char * queue_name);

        void send(const char * publish_string);

    private:
        //TODO(tim.simpson): What is the policy regarding ex and queue?
        //                   Do we need to free them somehow?
        AMQP amqp;
        AMQPExchange * ex;
        const std::string exchange_name;
        AMQPQueue * queue;
};

Sender::Sender(const char * host_name, const char * exchange_name,
               const char * queue_name)
:   amqp(host_name),
    ex(0),
    exchange_name(exchange_name),
    queue(0)
{
    ex = amqp.createExchange(exchange_name);
    ex->Declare(exchange_name, "direct");
    //Make sure the exchange was bound to the queue to actually send the messages thru
    queue = amqp.createQueue(queue_name);
    queue->Declare();
    queue->Bind(exchange_name, "");
}

void Sender::send(const char * publish_string) {
    ex->setHeader("Delivery-mode", 2);
    ex->setHeader("Content-type", "text/text");
    ex->setHeader("Content-encoding", "UTF-8");
    ex->Publish(publish_string, "");
}


int main(int argc, const char* argv[]) {
	if (argc < 2) {
		std::cerr << "Please specify the method name as the first argument."
		          << std::endl;
		return 1;
	}
	const char * method_name = argv[1];
    try {
        // daemon(1,0);
        Sender sender("guest:guest@localhost:5672/", "guest.hostname_exchange",
                      "guest.hostname");
        std::stringstream publish_string;
        publish_string << "{'method': '" << method_name << "'}";
        cout << publish_string.str() <<endl;
        sender.send(publish_string.str().c_str());
    } catch (AMQPException e) {
        std::cout << "An exception occured (code " << e.getReplyCode()
                  << "):" << e.getMessage() << std::endl;
    }
    return 0;
}
