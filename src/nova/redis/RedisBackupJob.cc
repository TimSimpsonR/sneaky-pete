#include "pch.hpp"
#include "RedisBackupJob.h"
#include "nova/Log.h"
#include "nova/redis/RedisClient.h"
#include "nova/redis/RedisException.h"
#include <boost/thread.hpp>
#include <time.h>

using nova::backup::BackupCreationArgs;
using nova::backup::BackupRunnerData;
using boost::optional;
using std::string;
using namespace nova::utils::swift;



namespace nova { namespace redis {

/*****************************************************************************
 * RdbPersistence
 *****************************************************************************/
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


/*****************************************************************************
 * RedisBackupJob
 *****************************************************************************/

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
    SwiftFileReader reader("/var/lib/redis/dump.rdb");
    return writer.write(reader);
}




// void RedisBackupJob::upload_to_swift() {
//     SwiftFileInfo file_info(backup_info.location, swift_container,
//                             backup_info.id);
//     SwiftUploader writer(token, segment_max_size, file_info,
//                          checksum_wait_time);
//     // Save the backup information to the database in case of failure
//     // This allows delete calls to clean up after failures.
//     // https://bugs.launchpad.net/trove/+bug/1246003
//     DbInfo pre_info = {
//             "",
//             "xtrabackup_v1",
//             file_info.manifest_url(),
//             stats->used
//     };
//     update_db("BUILDING", pre_info);

//     // Write the backup to swift.
//     // The checksum returned is the swift checksum of the concatenated
//     // segment checksums.
//     auto checksum = writer.write(reader);

//     // check the process was successful
//     if (!reader.successful()) {
//         update_db("FAILED");
//         NOVA_LOG_ERROR("Reader was unsuccessful! Setting backup to FAILED!");
//     } else {
//         // stats.used
//         DbInfo info = {
//             checksum,
//             "xtrabackup_v1",
//             file_info.manifest_url(),
//             stats->used
//         };
//         update_db("COMPLETED", info);
//     }
// }

// // /** Determines if we need to save the Redis data again. */
// // void save_again_if_necessary() {
// //     const auto save_time = get_last_save_time();
// //     const auto now = get_now();
// //     if (now - tolerance > save_time) {
// //         save();
// //     }
// // }

// // void wait_for_bgsave_or_aof_rewrite_to_finish();

// // void upload_to_swift();



} }
