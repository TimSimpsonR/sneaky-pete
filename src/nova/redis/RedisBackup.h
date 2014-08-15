#ifndef __NOVA_REDIS_REDISBACKUP_H
#define __NOVA_REDIS_REDISBACKUP_H

#include "nova/backup/BackupManager.h"
#include "nova/backup/BackupRestore.h"
#include "nova/process.h"
#include "nova/redis/RedisClient.h"
#include "nova/utils/regex.h"
#include <boost/shared_ptr.hpp>
#include <string>


namespace nova { namespace redis {

std::string find_dump_file(RedisClient & client);

/** Allows the RDB file to be refreshed. */
class RdbPersistence {
public:
    RdbPersistence(long long tolerance);

    std::string get_dump_file();

    void update();

private:
    RedisClient client;
    long long tolerance;

    bool bgsave_or_aof_rewrite_ocurring();

    void save();

    bool wait_for_save();
};




/** Safely runs a backup depending on if it's a slave by refreshing the rdb
 *  dump file and uploading it to Swift. */
class RedisBackupJob : public nova::backup::BackupJob {
public:

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


class RedisBackupManager : public nova::backup::BackupManager {
    public:
        RedisBackupManager(const nova::backup::BackupManagerInfo & info,
                           const long long tolerance);

        virtual ~RedisBackupManager();

        template<typename Flags, typename... Args>
        static nova::backup::BackupManagerPtr from_flags(
            const Flags & flags, Args & ... args)
        {
            using nova::backup::BackupManagerInfo;
            using nova::backup::BackupManagerPtr;

            BackupManagerInfo info
                = BackupManagerInfo::from_flags(flags, args...);
            const long long tolerance
                = flags.redis_backup_rdb_max_age_in_seconds();
            BackupManagerPtr result;
            result.reset(new RedisBackupManager(info, tolerance));
            return result;
        }

        virtual void run_backup(
            const nova::backup::BackupCreationArgs & args);

    private:
        // How recent the dump file must be before we begin.
        long long tolerance;
};


class RedisBackupRestoreManager : public nova::backup::BackupRestoreManager {
    public:
        RedisBackupRestoreManager();

        template<typename Flags>
        static nova::backup::BackupRestoreManagerPtr from_flags(
            const Flags & flags)
        {
            nova::backup::BackupRestoreManagerPtr ptr(
                new RedisBackupRestoreManager());
            return ptr;
        }

        virtual void run(const nova::backup::BackupRestoreInfo & restore);



    private:
        class Job;
};

} }  // end nova::redis

#endif
