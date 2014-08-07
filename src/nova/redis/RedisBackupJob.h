#ifndef __NOVA_REDIS_REDISBACKUPJOB_H
#define __NOVA_REDIS_REDISBACKUPJOB_H

namespace nova { namespace redis {

//TODO(tim.simpson): Pretty obvious.

template<typename ClientType>
class RedisBackupJob : public nova::utils::Job {
public:
    ClientType client;

    RedisBackupJob()
    : client()
    {
    }

    virtual ~RedisBackupJob() {
    }

    virtual nova::utils::Job * clone() const {
        return new RedisBackupJob();
    }

    // bool bgsave_or_aof_rewrite_ocurring();

    virtual void operator()() {
        if (!i_am_a_slave()) {
            NOVA_LOG_INFO("We're not a slave, so avoid the backup.");
            return;  // Don't do this unless it's a slave!
        }
        NOVA_LOG_INFO("We're a slave. Performing backup.");

        // if (bgsave_or_aof_rewrite_ocurring()) {
        //     wait_for_bgsave_or_aof_rewrite_to_finish();
        // }
        // save_again_ig
        // upload_to_swift();
    }

    // /** True if the datastore is a slave. */
    bool i_am_a_slave() {
 //       client
        auto res = client.info();
        NOVA_LOG_INFO("WHAT THE HECK? %s", res.status);
        return true;
    }

    // void save() {
    //     // Check memory, go with BGSAVE if there's enough memory
    //     // call save since we're a slave
    //     client.bgsave();
    // }

    // /** Determines if we need to save the Redis data again. */
    // void save_again_if_necessary() {
    //     const auto save_time = get_last_save_time();
    //     const auto now = get_now();
    //     if (now - tolerance > save_time) {
    //         save();
    //     }
    // }

    // void wait_for_bgsave_or_aof_rewrite_to_finish();

    // void upload_to_swift();
};


} }

#endif
