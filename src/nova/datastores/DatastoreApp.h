#ifndef __NOVA_DATASTORES_DATASTOREAPP_H
#define __NOVA_DATASTORES_DATASTOREAPP_H

#include <list>
#include "nova/db/mysql.h"
#include "nova/rpc/sender.h"
#include "nova/backup/BackupRestore.h"
#include <boost/optional.hpp>
#include <boost/shared_ptr.hpp>
#include <string>


// Forward declaration
namespace nova { namespace guest { namespace common {
    class PrepareHandler;
}}}

namespace nova { namespace datastores {


    class DatastoreApp {
        // Let prepare handler call this method.
        friend class nova::guest::common::PrepareHandler;

        protected:
            virtual void prepare(
                const boost::optional<std::string> & root_password,
                const std::string & config_contents,
                const boost::optional<std::string> & overrides,
                boost::optional<nova::backup::BackupRestoreInfo> restore
            ) = 0;
    };

    typedef boost::shared_ptr<DatastoreApp> DatastoreAppPtr;

} }

#endif
