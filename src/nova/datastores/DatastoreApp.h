#ifndef __NOVA_DATASTORES_DATASTOREAPP_H
#define __NOVA_DATASTORES_DATASTOREAPP_H

#include <list>
#include "nova/db/mysql.h"
#include "nova/rpc/sender.h"
#include "nova/guest/backup/BackupRestore.h"
#include <boost/optional.hpp>
#include <boost/shared_ptr.hpp>
#include <string>


namespace nova { namespace datastores {

    class DatastoreApp {
        public:
            virtual void prepare(
                const std::string & config_contents,
                const boost::optional<std::string> & overrides,
                boost::optional<nova::guest::backup::BackupRestoreInfo> restore
            ) = 0;
    };

    typedef boost::shared_ptr<DatastoreApp> DatastoreAppPtr;

} }

#endif
