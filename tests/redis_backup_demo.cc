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
#include "nova/redis/RedisBackup.h"
#include "nova/redis/RedisBackupJob.h"

using namespace std;
using namespace boost;
using namespace boost::assign;
using nova::Log;
using nova::LogApiScope;
using nova::LogOptions;

using nova::utils::Curl;
using nova::utils::CurlScope;

using namespace nova::redis;

using namespace nova::utils::swift;
using namespace nova::redis;

int main(int argc, char **argv)
{
    LogApiScope log(LogOptions::simple());
    CurlScope scope;

    //RedisBackupJob<Client> job;

    //job();

    return 0;
}
