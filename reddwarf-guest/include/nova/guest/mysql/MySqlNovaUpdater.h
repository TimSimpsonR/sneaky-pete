#ifndef __NOVA_GUEST_MYSQL_MYSQLNOVAUPDATER_H
#define __NOVA_GUEST_MYSQL_MYSQLNOVAUPDATER_H

#include "nova/guest/guest.h"
#include "nova/guest/mysql.h"


namespace nova { namespace guest { namespace mysql {

    /* Talks back to Nova, gives it updates. */
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

            MySqlNovaUpdater(MySqlConnectionPtr nova_db_connection,
                             const char * guest_ethernet_device);

            int get_guest_instance_id();

            Status get_local_db_status() const;

            static const char * status_name(Status status);

            void update_status(Status status);

        private:
            std::string guest_ethernet_device;

            boost::optional<std::string> find_mysql_pid_file() const;

            MySqlConnectionPtr nova_db;
    };

    typedef boost::shared_ptr<MySqlNovaUpdater> MySqlNovaUpdaterPtr;

} } }

#endif
