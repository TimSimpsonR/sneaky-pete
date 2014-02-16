#include "nova/Log.h"
#include "nova/rpc/sender.h"
#include <boost/format.hpp>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

using namespace std;
using namespace boost;
using nova::JsonData;
using nova::Log;
using nova::LogApiScope;
using nova::LogOptions;

// Repeatedly send messages to the queue.
// ./send_forever guest f7999d1955c5014aa32c guest restests

//using namespace nova;
using namespace nova::rpc;

int main(int argc, char **argv)
{
  LogApiScope log(LogOptions::simple());

  if (argc < 4) {
    cerr << "Usage: send_forever <user> <password> <topic> <exchange>" << endl;
    return 1;
  }

  int i = 0;
  const char * userid = argv[++i];
  const char * password = argv[++i];
  const char * topic = argv[++i];
  const char * exchange = argv[++i];

  const int sleep_seconds = 10;

  const char * MESSAGE = "{\"oslo.message\": \"{"
                             "\\\"method\\\": \\\"forever\\\", "
                             "\\\"args\\\": {}}\"}";

  ResilientSender sender("localhost", 5672, userid, password, 4096, topic, exchange, 10);

  while(true){
        sender->send(MESSAGE);
        sleep(sleep_seconds);
  }

  return 0;
}
