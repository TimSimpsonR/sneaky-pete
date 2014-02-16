#ifndef __NOVA_GUEST_MYSQL_MYSQLNOVAUPDATER_H
#define __NOVA_GUEST_MYSQL_MYSQLNOVAUPDATER_H

#include <list>
#include "nova/db/mysql.h"
#include "nova/rpc/sender.h"
#include <boost/thread/mutex.hpp>
#include <boost/optional.hpp>
#include <memory>
#include <sstream>
#include <string>
#include "nova/utils/subsecond.h"


namespace nova { namespace guest { namespace mysql {

    class MySqlAppStatusTestsFixture;

    struct MySqlAppStatusContext {

        virtual ~MySqlAppStatusContext();

        virtual void execute(std::stringstream & out,
                             const std::list<std::string> & cmds) const;
        virtual bool is_file(const char * file_path) const;
    };

    /** Answers the question "what is the status of the MySQL application on
     *  this box?" The answer can be that the application is not installed, or
     *  the state of the application is determined by calling a series of
     *  commands.
     *  This class also handles saving and load the status of the MySQL app
     *  in the Nova database.
     *  The status is updated whenever the update() method is called, except
     *  if the state is changed to building or restart mode using the
     *  "begin_mysql_install" and "begin_mysql_restart" methods.
     *  The building mode persists in the Nova DB while restarting mode does
     *  not (so if there is a Sneaky Pete crash update() will set the status to
     *  show a failure).
     *  These modes are exited and functionality to update() returns when
     *  end_install_or_restart() is called, at which point the status again
     *  reflects the actual status of the MySQL app.
     */
    class MySqlAppStatus {
        friend class MySqlAppStatusTestsFixture;

        struct MySqlAppStatusFunctor {
            virtual void operator()() = 0;
        };

        public:
            /* NOTE: These *MUST* match the values found in
             * trove.instance.models.ServiceStatuses! */
            enum Status {
                RUNNING = 0x01, // MySQL is active.
                BLOCKED = 0x02,
                PAUSED = 0x03,  // Instance is rebooting (this is set in Nova).
                SHUTDOWN = 0x04, // MySQL is not running.
                CRASHED = 0x06, // When MySQL died after starting.
                FAILED = 0x08,
                BUILDING = 0x09, // MySQL is being installed / prepared.
                UNKNOWN=0x16,  // Set when the guest becomes unresponsive
                NEW = 0x17 // Set when the instance is shiny and new.
            };

            MySqlAppStatus(nova::rpc::ResilientSenderPtr sender,
                           const char * guest_id,
                           MySqlAppStatusContext * context
                                 = new MySqlAppStatusContext());

            /** Called right before MySql is prepared. */
            void begin_mysql_install();

            /* Called before restarting MYSQL. */
            void begin_mysql_restart();

            /* Called when MySQL failed to install. Updates Nova DB to
             * FAILED. */
            void end_failed_install();

            /* Called after MySQL is installed or restarted. Updates the Nova
             * DB with the actual MySQL status. */
            void end_install_or_restart();

            /** Useful for diagnostics and logging. */
            const char * get_current_status_string() const;

            /* Returns true if the MySQL application should be installed and
             * an attempt to ascertain its status won't result in nonsense. */
            bool is_mysql_installed() const;

            /* Returns true iff the MySQL application is running. */
            bool is_mysql_running() const;

            /** Returns a readable string for each status enum. */
            static const char * status_name(Status status);

            /** Find and report the status of MySQL on this machine.
             *  The Nova DB is updated and the status is also returned. */
            void update();

            /** Waits for the given time for the real status to change to the
             *  one specified. Does not update the publicly viewable status
             *  unless "update_db" is true. */
            bool wait_for_real_state_to_change_to(Status status, int max_time,
                                                  bool update_db=false);

        protected:

            /** Gets the status of the MySQL app on this machine.
             *  Note: This method produces nonsense (SHUTDOWN) if MySQL is not
             *  installed or the installation procedure failed. */
            Status get_actual_db_status() const;

            /** If true, updates are restricted until the mode is switched
             *  off. */
            bool is_mysql_restarting() const;

            /** Changes the status of the MySQL app in the Nova DB. */
            void set_status(Status status);

        private:
            MySqlAppStatus(MySqlAppStatus const &);
            MySqlAppStatus & operator = (const MySqlAppStatus &);

            std::auto_ptr<MySqlAppStatusContext> context;

            boost::optional<std::string> find_mysql_pid_file() const;

            const char * guest_id;

            bool restart_mode;

            boost::optional<Status> status;

            boost::mutex status_mutex;

            nova::rpc::ResilientSenderPtr sender;
    };

    typedef boost::shared_ptr<MySqlAppStatus> MySqlAppStatusPtr;

} } }

#endif
