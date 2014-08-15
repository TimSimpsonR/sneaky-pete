#include "pch.hpp"
#include "RedisBackup.h"
#include "nova/Log.h"
#include "nova/redis/RedisClient.h"
#include "nova/redis/RedisException.h"
#include "nova/process.h"
#include <boost/thread.hpp>
#include <time.h>

using nova::backup::BackupCreationArgs;
using nova::backup::BackupManagerInfo;
using nova::backup::BackupRunnerData;
using nova::backup::BackupRestoreInfo;
using nova::process::CommandList;
using boost::format;
using boost::optional;
using std::string;
using namespace nova::utils::swift;


namespace nova { namespace redis {

namespace {


}  // end anonymous namespace


string find_dump_file(RedisClient & client) {
    return client.info().get_optional("dbfilename")
        .get_value_or("/var/lib/redis/dump.rdb");
}

/**---------------------------------------------------------------------------
 *- RdbPersistence
 *---------------------------------------------------------------------------*/

RdbPersistence::RdbPersistence(long long tolerance)
:   client(),
    tolerance(tolerance) {
}


bool RdbPersistence::bgsave_or_aof_rewrite_ocurring() {
    auto info = client.info();
    return (info.get("rdb_bgsave_in_progress") != "0"
            || info.get("aof_rewrite_in_progress") != "0");
}

void RdbPersistence::save() {
    // TODO(tim.simpson): Check memory, go with BGSAVE if there's enough memory
    // call save since we're a slave
    client.command("BGSAVE").expect_status();
}

void RdbPersistence::update() {
    NOVA_LOG_INFO("Performing backup.");

    const boost::posix_time::seconds one_second(1);
    bool wait_count = 0;
    while (bgsave_or_aof_rewrite_ocurring()) {
        if (0 == wait_count % 60) {
            NOVA_LOG_INFO("aof rewrite or bgsave in progress, waiting...");
        } else {
            NOVA_LOG_TRACE("aof rewrite or bgsave in progress, waiting...");
        }
        boost::this_thread::sleep(one_second);
        ++ wait_count;
    }

    save();

    //TODO(tim.simpson): Have some kind of time out here for the bgsave.
    while(wait_for_save()) {
        NOVA_LOG_TRACE("Still saving...");
        boost::this_thread::sleep(one_second);
    }


    // Check to see that LASTSAVE is very recent.
    // save_again_ig
    // upload_to_swift();
}

bool RdbPersistence::wait_for_save() {
    //TODO: Move
    const auto last_save = client.command("LASTSAVE").expect_int();
    const auto now = ::time(NULL);

    NOVA_LOG_INFO("LASTSAVE? %s", last_save);
    NOVA_LOG_INFO("NOW?      %s", now);

    long long diff = now - last_save;
    NOVA_LOG_INFO("   diff = %s", diff);
    return (diff > tolerance);
}


/**---------------------------------------------------------------------------
 *- RedisBackupJob
 *---------------------------------------------------------------------------*/

RedisBackupJob::RedisBackupJob(BackupRunnerData data, long long tolerance,
                               const BackupCreationArgs & args)
:   BackupJob(data, args),
    client(),
    tolerance(tolerance) {
}

RedisBackupJob::RedisBackupJob(const RedisBackupJob & other)
:   BackupJob(other.data, other.args), client(), tolerance(other.tolerance) {
}

RedisBackupJob::~RedisBackupJob() {
}

void RedisBackupJob::operator()() {
    try {
        update_trove_to_building();
        update_trove_to_completed(run());
    } catch(const std::exception ex) {
        update_trove_to_failed();
    }
}

nova::utils::Job * RedisBackupJob::clone() const {
    return new RedisBackupJob(data, tolerance, args);
}

const char * RedisBackupJob::get_backup_type() const {
    return "rdb";
}

bool RedisBackupJob::is_slave() {
    const string role = client.info().get("role");
    return role == "slave";
}

string RedisBackupJob::run() {
    update_trove_to_building();
    if (!is_slave()) {
        NOVA_LOG_ERROR("We're not a slave, so avoid the backup.");
        throw RedisException(RedisException::INVALID_BACKUP_TARGET);
    }
    RdbPersistence rdb(tolerance);
    rdb.update();

    NOVA_LOG_INFO("Uploading to Swift.");
    return upload();
}


string RedisBackupJob::upload() {
    SwiftUploader writer(args.token, data.segment_max_size, file_info,
                         data.checksum_wait_time);
    const string dump_file = find_dump_file(client);
    LocalFileReader reader(dump_file.c_str());
    return writer.write(reader);
}



/**---------------------------------------------------------------------------
 *- RedisBackupManager
 *---------------------------------------------------------------------------*/

RedisBackupManager::RedisBackupManager(const BackupManagerInfo & info,
                                       const long long tolerance)
:   BackupManager(info),
    tolerance(tolerance)
{

}


RedisBackupManager::~RedisBackupManager() {

}

void RedisBackupManager::run_backup(const BackupCreationArgs & args) {
    NOVA_LOG_INFO("Starting backup for tenant %s, backup_id=%d",
                   args.tenant.c_str(), args.id.c_str());
    RedisBackupJob job(data, tolerance, args);
    runner.run(job);
}


/**---------------------------------------------------------------------------
 *- RedisBackupRestoreManager
 *---------------------------------------------------------------------------*/


RedisBackupRestoreManager::RedisBackupRestoreManager() {
}

#define SHELL(msg, arg) { \
    const string cmd = str(format(msg) % arg); \
    NOVA_LOG_INFO(cmd.c_str()); \
    nova::process::shell(cmd.c_str(), false); \
}

void RedisBackupRestoreManager::run(const BackupRestoreInfo & restore) {
    RedisClient client;
    const string dump_file = find_dump_file(client);

    SHELL("sudo rm %s", dump_file);

    LocalFileWriter local_dump_file(dump_file.c_str());
    SwiftDownloader downloader(restore.get_token(),
                               restore.get_backup_url(),
                               restore.get_backup_checksum());
    downloader.read(local_dump_file);

    SHELL("sudo chown redis:redis %s", dump_file);
    SHELL("sudo chmod 033 %s", dump_file);
}

} } // end namespace redis
