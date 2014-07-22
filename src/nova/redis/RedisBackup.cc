#ifndef __NOVA_REDIS_REDISBACKUP_CC
#define __NOVA_REDIS_REDISBACKUP_CC

#include "pch.hpp"
#include "RedisBackup.h"

using nova::backup::BackupInfo;
using nova::backup::BackupRestoreInfo;
using nova::process::CommandList;
using nova::backup::BackupManagerInfo;
using std::string;


namespace nova { namespace redis {

namespace {


}  // end anonymous namespace


/**---------------------------------------------------------------------------
 *- RedisBackupManager::Job
 *---------------------------------------------------------------------------*/

class RedisBackupManager::Job : public nova::utils::Job {
public:
    virtual void execute() {

    }
};


/**---------------------------------------------------------------------------
 *- RedisBackupManager
 *---------------------------------------------------------------------------*/

RedisBackupManager::RedisBackupManager(const BackupManagerInfo & info,
                                       const CommandList commands)
:   BackupManager(info),
    commands(commands)
{

}


RedisBackupManager::~RedisBackupManager() {

}

void RedisBackupManager::run_backup(const string & tenant,
                                    const string & token,
                                    const BackupInfo & backup_info) {
    NOVA_LOG_INFO("Starting backup for tenant %s, backup_id=%d",
                   tenant.c_str(), backup_info.id.c_str());
    #ifdef _DEBUG
        NOVA_LOG_INFO("Token = %s", token.c_str());
    #endif

    /*BackupJob job(sender, commands, interrogator, segment_max_size,
                  checksum_wait_time,
                  swift_container, time_out, tenant, token,
                  zlib_buffer_size, backup_info);
    runner.run(job);*/
}


/**---------------------------------------------------------------------------
 *- RedisBackupRestoreManager::Job
 *---------------------------------------------------------------------------*/

class RedisBackupRestoreManager::Job {
public:
    void execute() {

    }
};


/**---------------------------------------------------------------------------
 *- RedisBackupRestoreManager
 *---------------------------------------------------------------------------*/


RedisBackupRestoreManager::RedisBackupRestoreManager(
    const CommandList command_list,
    const size_t zlib_buffer_size)
:   commands(command_list),
    zlib_buffer_size(zlib_buffer_size)
{
}

void RedisBackupRestoreManager::run(const BackupRestoreInfo & restore) {
    Job job; //*this, restore);
    job.execute();
}

} } // end namespace redis

#endif