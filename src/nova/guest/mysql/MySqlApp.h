#ifndef __NOVA_GUEST_MYSQL_MYSQLAPP_H
#define __NOVA_GUEST_MYSQL_MYSQLAPP_H

#include "nova/datastores/DatastoreApp.h"
#include "nova/guest/mysql/MySqlAdmin.h"
#include "nova/guest/mysql/MySqlAppStatus.h"
#include "nova/backup/BackupRestore.h"
#include <boost/optional.hpp>
#include <vector>


namespace nova { namespace guest { namespace mysql {

/** This class maintains the MySQL application on this machine.
 *  Administrative tasks performed not on the MySQL database but the actual
 *  application itself are performed using this. */
class MySqlApp : public nova::datastores::DatastoreApp {
    friend class MySqlAppTestsFixture;

    public:

        MySqlApp(MySqlAppStatusPtr status,
                 nova::backup::BackupRestoreManagerPtr
                    backup_restore_manager,
                 int state_change_wait_time,
                 bool skip_install_for_prepare);

        virtual ~MySqlApp();

        // Removes the overrides file.
        void remove_overrides();

        void start_db_with_conf_changes(const std::string & config_contents);

        void reset_configuration(const std::string & config_contents);

        /** Writes an optional overrides file which lives near the normal
         *  my.cnf. */
        void write_config_overrides(const std::string & overrides_content);

        void write_ssl_files(
            const std::string & ca_certificate,
            const std::string & private_key,
            const std::string & public_key
        );

    protected:
        /** Installs MySql, secures it, and possibly runs a backup. */
        virtual void prepare(
            const boost::optional<std::string> & root_password,
            const std::string & config_contents,
            const boost::optional<std::string> & overrides,
            boost::optional<nova::backup::BackupRestoreInfo> restore
        );

        virtual void enable_ssl(
            const std::string & ca_certificate,
            const std::string & private_key,
            const std::string & public_key
        );

        virtual void specific_start_app_method();

        virtual void specific_stop_app_method();

    private:

        // Do not implement.
        MySqlApp(const MySqlApp &);
        MySqlApp & operator = (const MySqlApp &);

        nova::backup::BackupRestoreManagerPtr backup_restore_manager;

        /** Install the set of mysql my.cnf templates from dbaas-mycnf package.
         *  The package generates a template suited for the current
         *  container flavor. Update the os_admin user and password
         * to the my.cnf file for direct login from localhost
         **/
        void write_mycnf(const std::string & config_contents,
                         const boost::optional<std::string> & overrides,
                         const boost::optional<std::string> & admin_password_arg);

        /** Stop mysql. Only update the DB if update_db is true. */
        //void internal_stop_mysql(bool update_db=false);

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

        /* Tests that a connection can be made using the my.cnf settings. */
        static bool test_ability_to_connect(bool throw_on_failure);

        void wait_for_initial_connection();

        void wait_for_mysql_initial_stop();

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
