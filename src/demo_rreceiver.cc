#include "nova/Log.h"
#include "nova/rpc/receiver.h"
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

// Repeatedly recieve and print messages on a specific topic.
// ./demo_rreceiver guest f7999d1955c5014aa32c guest restests

//using namespace nova;
using namespace nova::rpc;

int main(int argc, char **argv)
{
  LogApiScope log(LogOptions::simple());

  if (argc < 4) {
    cerr << "Usage: demo_rreceiver <user> <password> <topic> <exchange>" << endl;
    return 1;
  }

  int i = 0;
  const char * userid = argv[++i];
  const char * password = argv[++i];
  const char * topic = argv[++i];
  const char * exchange = argv[++i];

  ResilientReceiver receiver("localhost", 5672, userid, password, 4096, topic, exchange, 10);

  while(true){
        nova::guest::GuestInput input = receiver.next_message();
        cout << input.args->to_string() << endl;

        nova::guest::GuestOutput output;
        output.failure = boost::none;
        output.result = JsonData::from_string("ok");

        receiver.finish_message(output);
  }

  return 0;
}

