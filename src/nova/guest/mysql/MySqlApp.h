#ifndef __NOVA_GUEST_MYSQL_MYSQLAPP_H
#define __NOVA_GUEST_MYSQL_MYSQLAPP_H

#include "nova/guest/apt.h"
#include "nova/guest/mysql/MySqlAdmin.h"
#include "nova/guest/mysql/MySqlAppStatus.h"
#include "nova/guest/backup/BackupRestore.h"
#include <boost/optional.hpp>

namespace nova { namespace guest { namespace mysql {

/** This class maintains the MySQL application on this machine.
 *  Administrative tasks performed not on the MySQL database but the actual
 *  application itself are performed using this. */
class MySqlApp {
    friend class MySqlAppTestsFixture;

    public:

        MySqlApp(MySqlAppStatusPtr status,
                 nova::guest::backup::BackupRestoreManagerPtr
                    backup_restore_manager,
                 int state_change_wait_time,
                 bool skip_install_for_prepare);

        virtual ~MySqlApp();

        /** Installs MySql, secures it, and possibly runs a backup. */
        void prepare(
            nova::guest::apt::AptGuest & apt,
            const std::string & config_location,
            const std::string & config_contents,
            boost::optional<nova::guest::backup::BackupRestoreInfo> restore
        );

        /** Restarts MySQL on this host. */
        void restart();

        void start_db_with_conf_changes(nova::guest::apt::AptGuest & apt,
                                        const std::string & config_location,
                                        const std::string & config_contents);

        void reset_configuration(nova::guest::apt::AptGuest & apt,
                                 const std::string & config_location,
                                 const std::string & config_contents);

        /** Stops MySQL on this host. */
        void stop_db(bool do_not_start_on_reboot=false);


    private:

        // Do not implement.
        MySqlApp(const MySqlApp &);
        MySqlApp & operator = (const MySqlApp &);

        nova::guest::backup::BackupRestoreManagerPtr backup_restore_manager;

        void create_admin_user(MySqlAdmin & sql,
                               const std::string & password);

        /** Generate and set a random root password and forget about it. */
        void generate_root_password(MySqlAdmin & sql);

        /** Install the set of mysql my.cnf templates from dbaas-mycnf package.
         *  The package generates a template suited for the current
         *  container flavor. Update the os_admin user and password
         * to the my.cnf file for direct login from localhost
         **/

        void write_mycnf(nova::guest::apt::AptGuest & apt,
                         const std::string & config_location,
                         const std::string & config_contents,
                         const boost::optional<std::string> & admin_password_arg);

        /*
         * Just installs MySQL, but doesn't secure it.
         */
        void install_mysql(nova::guest::apt::AptGuest & apt);

        /** Stop mysql. Only update the DB if update_db is true. */
        void internal_stop_mysql(bool update_db=false);

        /** Helps secure the MySQL install by removing the anonymous user. */
        void remove_anonymous_user(MySqlAdmin & sql);

        /** Helps secure the MySQL install by removing remote root access. */
        void remove_remote_root_access(MySqlAdmin & sql);

        /** Stops MySQL and restarts it, wiping the ib_logfiles in-between.
         *  This should never be done unless the innodb_log_file_size changes.*/
        void restart_mysql_and_wipe_ib_logfiles();

        /* Runs mysqld_safe with the init-file option to set up the database. */
        void run_mysqld_with_init();

        const bool skip_install_for_prepare;

        const int state_change_wait_time;

        MySqlAppStatusPtr status;

        /** Starts MySQL on this host. */
        void start_mysql(bool update_db=false);

        /* Tests that a connection can be made using the my.cnf settings. */
        static bool test_ability_to_connect(bool throw_on_failure);

        void wait_for_initial_connection();

        void wait_for_mysql_initial_stop();

        void wait_for_internal_stop(bool update_db);

        void wipe_ib_logfile(const int index);

        /* Destroy these files as needed for my.cnf changes. */
        void wipe_ib_logfiles();

        /* Writes an init file containing commands to prepare this MySQL
         * application for use by the agent. This init file will be read and
         * each command executed as root by MySQL when 'run_mysqld_with_init'
         * is called. */
        void write_fresh_init_file(const std::string & admin_password,
                                   bool wipe_root_and_anonymous_users);

};

typedef boost::shared_ptr<MySqlApp> MySqlAppPtr;

} } }  // end nova::guest::mysql

#endif
