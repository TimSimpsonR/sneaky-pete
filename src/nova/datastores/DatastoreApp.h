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

        public:
            DatastoreApp(const char * const service_name);

            virtual ~DatastoreApp();

        protected:
            virtual void prepare(
                const boost::optional<std::string> & root_password,
                const std::string & config_contents,
                const boost::optional<std::string> & overrides,
                boost::optional<nova::backup::BackupRestoreInfo> restore
            ) = 0;

            class StartOnBoot {
                public:
                    StartOnBoot(const char * const service_name);
                    void disable_or_throw();
                    void enable_maybe();
                    void enable_or_throw();
                private:
                    const char * const service_name;

                    void call_update_rc(bool enabled,
                                        bool throw_on_bad_exit_code);
            };

            StartOnBoot start_on_boot;



    };

    typedef boost::shared_ptr<DatastoreApp> DatastoreAppPtr;


} }

#endif
