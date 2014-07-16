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

        void start_with_conf_changes(const std::string & config_contents);

    protected:
        virtual void prepare(
                const boost::optional<std::string> & root_password,
                const std::string & config_contents,
                const boost::optional<std::string> & overrides,
                boost::optional<nova::backup::BackupRestoreInfo> restore
            );

        virtual void specific_start_app_method();

        virtual void specific_stop_app_method();
};

typedef boost::shared_ptr<RedisApp> RedisAppPtr;

} }

#endif
