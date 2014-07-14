#ifndef __NOVA_REDIS_REDISAPP_H
#define __NOVA_REDIS_REDISAPP_H

#include "nova/redis/RedisAppStatus.h"
#include "nova/datastores/DatastoreApp.h"
#include "nova/backup/BackupRestore.h"
#include <boost/optional.hpp>
#include <boost/shared_ptr.hpp>
#include <vector>

namespace nova { namespace redis {

class RedisApp : public nova::datastores::DatastoreApp {
    public:
        RedisApp(RedisAppStatusPtr app_status,
                 const int state_change_wait_time);
        virtual ~RedisApp();

        void change_password(const std::string & password);

        /** Restart Redis. */
        void restart();

        void start_with_conf_changes(const std::string & config_contents);

        /** Shutdown Redis. */
        void stop();

    protected:
        virtual void prepare(
                const boost::optional<std::string> & root_password,
                const std::string & config_contents,
                const boost::optional<std::string> & overrides,
                boost::optional<nova::backup::BackupRestoreInfo> restore
            );
    private:
        RedisAppStatusPtr app_status;

        /** Stop Redis. Only update Trove if the argument is true. */
        void internal_stop(const bool update_trove=false);

        /** Start Redis. Only update Trove if the argument is true. */
        void internal_start(const bool update_trove=false);

        const int state_change_wait_time;

        /** Wait around until Redis is running. */
        void wait_for_internal_start(const bool update_trove);

        /** Wait around until the Redis app has been shut down. */
        void wait_for_internal_stop(const bool update_trove);
};

typedef boost::shared_ptr<RedisApp> RedisAppPtr;

} }

#endif
