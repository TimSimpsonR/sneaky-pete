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
#include "nova/redis/RedisBackup.h"
#include "nova/redis/RedisBackupJob.h"

#include "nova/utils/regex.h"

using namespace std;
using namespace boost;
using namespace boost::assign;
using nova::Log;
using nova::LogApiScope;
using nova::LogOptions;

using nova::utils::Curl;
using nova::utils::CurlScope;

using namespace nova::backup;
using namespace nova::redis;

using namespace nova::utils::swift;
using namespace nova::utils;
using namespace nova::redis;


int main(int argc, char **argv)
{
    const long long tolerance = 4;
    LogApiScope log(LogOptions::simple());
    CurlScope scope;

    if (argc < 2) {
        NOVA_LOG_ERROR("Usage: %s [info|rdb|all|dbfilename]",
                       (argc < 1 ? "program" : argv[0]));
        return 1;
    }
    string option = argv[1];
    if (option == "info") {
        RedisClient client;
        const auto string_info = client.command("INFO").expect_string();
        NOVA_LOG_INFO("Redis Info:\n%s", string_info);
        return 0;
    } else if (option == "dbfilename") {
        RedisClient client;
        NOVA_LOG_INFO("Redis dump file located at: %s", find_rdb_file(client));
        return 0;
    } else if (option == "rdb") {
        NOVA_LOG_INFO("Updating rdb.");
        RdbPersistence rdb(tolerance);
        rdb.update();
        return 0;
    } else if (option == "all") {
        NOVA_LOG_INFO("Running entire backup.");
        nova::rpc::ResilientSenderPtr null;
        nova::guest::diagnostics::Interrogator interrogator("/var/lib/redis");
        BackupRunnerData data = {
            null,
            interrogator,
            1024 * 1024,        // max backup segment size
            30,                 // checksum wait time
            "TEST_CONTAINER",   // container
            300                 // Timeout
        };
        const string tenant = "1000";
        if (argc < 3) {
            NOVA_LOG_ERROR("Expected token for arg 3");
            return 1;
        }
        const string token = argv[2];
        const BackupCreationArgs backup_info = {
            "tenant",
            "token",
            "id",
            "swift-url"
        };
        RedisBackupJob job(data, true, tolerance, backup_info);
        job();
        return 0;
    } else {
        NOVA_LOG_ERROR("don't know how to handle %s.", option);
        return 1;
    }
}
