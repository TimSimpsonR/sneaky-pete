#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fstream>
#include <iostream>
#include "nova/utils/Curl.h"
#include <boost/format.hpp>
#include "nova/Log.h"
#include <boost/assign/list_of.hpp>
#include "nova/redis/RedisClient.h"

using namespace std;
using namespace boost;
using namespace boost::assign;
using nova::LogApiScope;
using nova::LogOptions;
using namespace nova::redis;


int main(int argc, char **argv)
{
    LogApiScope log(LogOptions::simple());
    NOVA_LOG_INFO("Creating a default style Redis client.");
    NOVA_LOG_INFO("This will only work in a container.");
    RedisClient client;
    client.ping();
    NOVA_LOG_INFO("THE END");
}
