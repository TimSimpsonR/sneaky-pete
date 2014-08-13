#ifndef __NOVA_REDIS_REDISBACKUPJOB_H
#define __NOVA_REDIS_REDISBACKUPJOB_H


#include "nova/redis/RedisClient.h"
#include "nova/backup/BackupManager.h"
#include <boost/thread.hpp>
#include "nova/utils/threads.h"


namespace nova { namespace redis {

/** Allows the RDB file to be refreshed. */
class RdbPersistence {
public:
    RdbPersistence(long long tolerance);

    void update();

private:
    RedisClient client;
    long long tolerance;

    bool bgsave_or_aof_rewrite_ocurring();

    void save();

    bool wait_for_save();
};


void upload_file_to_swift();


/** Safely runs a backup depending on if it's a slave by refreshing the rdb
 *  dump file and uploading it to Swift. */
class RedisBackupJob : public nova::backup::BackupJob {
public:
    //ResilientSenderPtr sender;

    /* Tolerance is in seconds. */
    // RedisBackupJob(nova::rpc::ResilientSenderPtr sender,
    //                Interrogator interrogator,
    //                const int segment_max_size, const int checksum_wait_time,
    //                const string & swift_container, const double time_out,
    //                const string & tenant, const string & token,
    //                const BackupInfo & backup_info,
    //                long long tolerance);
    RedisBackupJob(nova::backup::BackupRunnerData data, long long tolerance,
                   const nova::backup::BackupCreationArgs & args);

    RedisBackupJob(const RedisBackupJob & other);

    virtual ~RedisBackupJob();

    virtual nova::utils::Job * clone() const;

    virtual void operator()();

protected:
    virtual const char * get_backup_type() const;

private:
    RedisClient client;

    bool is_slave();

    std::string run();

    long long tolerance;

    std::string upload();
};


} }

#endif
