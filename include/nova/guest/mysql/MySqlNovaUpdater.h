#ifndef __NOVA_GUEST_MYSQL_MYSQLNOVAUPDATER_H
#define __NOVA_GUEST_MYSQL_MYSQLNOVAUPDATER_H

#include "nova/db/mysql.h"
#include <boost/optional.hpp>
#include <string>


namespace nova { namespace guest { namespace mysql {

    /* Deals with the issue of identity of this guest instance *as a resource*
     * for DBaaS (vs as a Nova node) and communicating the status of this
     * resource to Nova.  Its responsibilities include:
     *    * Determining the guest ID by figuring out the host IP
     *    * Finding and reporting the status of the instance of MySQL installed
     *      on this box.
     */
    class MySqlNovaUpdater {

        public:
            /* NOTE: These *MUST* match the values found in
             * nova.compute.power_state! */
            enum Status {
                RUNNING = 0x01,
                BLOCKED = 0x02,
                SHUTDOWN = 0x04,
                CRASHED = 0x06,
                BUILDING = 0x09
            };

            MySqlNovaUpdater(nova::db::mysql::MySqlConnectionPtr nova_db,
                             const char * guest_ethernet_device);

            int get_guest_instance_id();

            Status get_local_db_status() const;

            static const char * status_name(Status status);

            void update_status(Status status);

        private:
            std::string guest_ethernet_device;

            boost::optional<std::string> find_mysql_pid_file() const;

            nova::db::mysql::MySqlConnectionPtr nova_db;
    };

    typedef boost::shared_ptr<MySqlNovaUpdater> MySqlNovaUpdaterPtr;

} } }

#endif
