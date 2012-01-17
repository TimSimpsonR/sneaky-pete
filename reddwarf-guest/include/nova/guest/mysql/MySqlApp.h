#ifndef __NOVA_GUEST_MYSQL_MYSQLAPP_H
#define __NOVA_GUEST_MYSQL_MYSQLAPP_H

#include "nova/guest/apt.h"
#include "nova/guest/mysql/MySqlAdmin.h"
#include "nova/guest/mysql/MySqlAppStatus.h"

namespace nova { namespace guest { namespace mysql {

/** This class maintains the MySQL application on this machine.
 *  Administrative tasks performed not on the MySQL database but the actual
 *  application itself are performed using this. */
class MySqlApp {
    friend class MySqlAppTestsFixture;

    public:

        MySqlApp(MySqlAppStatusPtr status,
                 int state_change_wait_time);

        virtual ~MySqlApp();

        /** Installs MySql and secure it. */
        void install_and_secure(nova::guest::apt::AptGuest & apt);

        /** Restarts MySQL on this host. */
        void restart();

    protected:

        void create_admin_user(MySqlAdmin & sql,
                               const std::string & password);

        /** Generate and set a random root password and forget about it. */
        void generate_root_password(MySqlAdmin & sql);

        /** Install the set of mysql my.cnf templates from dbaas-mycnf package.
         *  The package generates a template suited for the current
         *  container flavor. Update the os_admin user and password
         * to the my.cnf file for direct login from localhost
         **/
        void init_mycnf(nova::guest::apt::AptGuest & apt,
                        const std::string & password);

        /*
         * Just installs MySQL, but doesn't secure it.
         */
        void install_mysql(nova::guest::apt::AptGuest & apt);

        /** Helps secure the MySQL install by removing the anonymous user. */
        void remove_anonymous_user(MySqlAdmin & sql);

        /** Helps secure the MySQL install by removing remote root access. */
        void remove_remote_root_access(MySqlAdmin & sql);

        /** Stops MySQL and restarts it, wiping the ib_logfiles in-between.
         *  This should never be done unless the innodb_log_file_size changes.*/
        void restart_mysql_and_wipe_ib_logfiles();

        void start_mysql();

        void stop_mysql();

    private:
        // Do not implement.
        MySqlApp(const MySqlApp &);
        MySqlApp & operator = (const MySqlApp &);

        int state_change_wait_time;

        MySqlAdminPtr sql;

        MySqlAppStatusPtr status;

};

typedef boost::shared_ptr<MySqlApp> MySqlAppPtr;

} } }  // end nova::guest::mysql

#endif
