#ifndef __NOVA_REDIS_REDISAPP_H
#define __NOVA_REDIS_REDISAPP_H

#include "nova/guest/redis/status.h"
#include "nova/datastores/DatastoreApp.h"
#include "nova/guest/backup/BackupRestore.h"
#include <boost/optional.hpp>
#include <boost/shared_ptr.hpp>
#include <vector>

namespace nova { namespace redis {

class RedisApp : public nova::datastores::DatastoreApp {
    public:
        RedisApp(RedisAppStatusPtr app_status);
        virtual ~RedisApp();

        void change_password(const std::string & password);

        void restart();

        void start_with_conf_changes(const std::string & config_contents);

        void stop();

    protected:
        virtual void prepare(
                const boost::optional<std::string> & root_password,
                const std::string & config_contents,
                const boost::optional<std::string> & overrides,
                boost::optional<nova::guest::backup::BackupRestoreInfo> restore
            );
    private:
        RedisAppStatusPtr app_status;
};

typedef boost::shared_ptr<RedisApp> RedisAppPtr;

} }

#endif
