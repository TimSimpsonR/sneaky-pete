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
using nova::Log;
using nova::LogApiScope;
using nova::LogOptions;

// Send messages directly to Conductor.
// ./send_to_conductor guest f7999d1955c5014aa32c '{"method": "heartbeat", "args": {"instance_id": "4f313363-2db9-4a8d-9843-db8bf1205410", "payload": {"service_status": "running"}, "sent": 0.0}}'

//using namespace nova;
using namespace nova::rpc;

int main(int argc, char **argv)
{
  LogApiScope log(LogOptions::simple());

  if (argc < 3) {
    cerr << "Usage: send_to_conductor <user> <password> <message>" << endl;
    return 1;
  }

  int i = 0;
  const char * userid = argv[++i];
  const char * password = argv[++i];
  const char * message = argv[++i];

  ResilientSender rs("10.0.0.1", 5672,
                     userid, password,
                     4096, "trove-conductor","", "my-instance-id", 10);
  rs.send_plain_string(message);

  return 0;
}
