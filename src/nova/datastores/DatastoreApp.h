#ifndef __NOVA_DATASTORES_DATASTOREAPP_H
#define __NOVA_DATASTORES_DATASTOREAPP_H

#include <list>
#include "nova/db/mysql.h"
#include "nova/rpc/sender.h"
#include "nova/datastores/DatastoreStatus.h"
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
            DatastoreApp(const char * const service_name,
                         DatastoreStatusPtr status,
                         const int state_change_wait_time);

            virtual ~DatastoreApp();

            void restart();

            /** Shut down the datastore app. */
            void stop(bool do_not_start_on_reboot=true);

        protected:
            DatastoreStatusPtr status;

            /** Starts the application. Updates Trove with the app status if
                update_trove=true. */
            void internal_start(const bool update_trove=false);

            /** Stops the application. Updates Trove with the app status if
                update_trove=true. */
            void internal_stop(const bool update_trove=false);

            virtual void prepare(
                const boost::optional<std::string> & root_password,
                const std::string & config_contents,
                const boost::optional<std::string> & overrides,
                boost::optional<nova::backup::BackupRestoreInfo> restore
            ) = 0;

            /** Implements datastore specific code to start the app. */
            virtual void specific_start_app_method() = 0;

            /** Implements datastore specific code to stop the app. */
            virtual void specific_stop_app_method() = 0;

            const int state_change_wait_time;

            /** Internal method to wait for the datastore to start. */
            void wait_for_internal_start(const bool update_trove=false);

            /** Internal method to wait for the datastore to stop. */
            void wait_for_internal_stop(const bool update_trove=false);

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
