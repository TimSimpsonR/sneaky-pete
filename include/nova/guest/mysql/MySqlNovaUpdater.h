#ifndef __NOVA_GUEST_MYSQL_MYSQLNOVAUPDATER_H
#define __NOVA_GUEST_MYSQL_MYSQLNOVAUPDATER_H

#include <list>
#include "nova/db/mysql.h"
#include <boost/thread/mutex.hpp>
#include <boost/optional.hpp>
#include <memory>
#include <sstream>
#include <string>


namespace nova { namespace guest { namespace mysql {

    class MySqlNovaUpdaterTestsFixture;

    struct MySqlNovaUpdaterContext {
        virtual void execute(std::stringstream & out,
                             const std::list<const char *> & cmds) const;
        virtual bool is_file(const char * file_path) const;
    };

    /* Updates the Nova infastructure database with the status of the
     * MySql instance.
     * As of this moment it should only be necessary to call "update" and
     * "mark_mysql_as_installed." The other public methods are only there for
     * unit testing.
     */
    class MySqlNovaUpdater {
        friend class MySqlNovaUpdaterTestsFixture;

        public:
            /* NOTE: These *MUST* match the values found in
             * nova.compute.power_state! */
            enum Status {
                RUNNING = 0x01,
                BLOCKED = 0x02,
                SHUTDOWN = 0x04,
                CRASHED = 0x06,
                FAILED = 0x08,
                BUILDING = 0x09
            };

            MySqlNovaUpdater(nova::db::mysql::MySqlConnectionPtr nova_db,
                             const char * nova_db_name,
                             const char * guest_ethernet_device,
                             boost::optional<int> preset_instance_id
                                 = boost::none,
                             MySqlNovaUpdaterContext * context
                                 = new MySqlNovaUpdaterContext());

            /** Called right before MySql is prepared. */
            void begin_mysql_install();

            /* Called once MySQL is known to be installed on this machine. */
            void mark_mysql_as_installed();

            /* Returns true if the MySQL application should be installed and
             * an attempt to ascertain its status won't result in nonsense. */
            bool mysql_is_installed();

            /** Returns a readable string for each status enum. */
            static const char * status_name(Status status);

            /** Find and report the status of MySQL on this machine. */
            void update();

        protected:

            /** Gets the status of the MySQL app on this machine.
             *  Note: This method produces nonsense (SHUTDOWN) if MySQL is not
             *  installed or the installation procedure failed. */
            Status get_actual_db_status() const;

            /** Determines the ID of this instance in the Nova DB. */
            int get_guest_instance_id();

            /** Changes the status of the MySQL app in the Nova DB. */
            void set_status(Status status);

        private:

            std::auto_ptr<MySqlNovaUpdaterContext> context;

            void ensure_db();

            boost::optional<std::string> find_mysql_pid_file() const;

            /** Talks to Nova DB to get the status of the MySQL app. */
            boost::optional<Status> get_status_from_nova_db();

            const std::string guest_ethernet_device;

            nova::db::mysql::MySqlConnectionPtr nova_db;

            std::string nova_db_name;

            boost::mutex nova_db_mutex;

            boost::optional<int> preset_instance_id;

            boost::optional<Status> status;
    };

    typedef boost::shared_ptr<MySqlNovaUpdater> MySqlNovaUpdaterPtr;

} } }

#endif
