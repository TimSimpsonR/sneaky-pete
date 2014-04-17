#ifndef __NOVA_DATASTORES_DATASTORESTATUS_H
#define __NOVA_DATASTORES_DATASTORESTATUS_H

#include <list>
#include "nova/db/mysql.h"
#include "nova/rpc/sender.h"
#include <boost/thread/mutex.hpp>
#include <boost/optional.hpp>
#include <memory>
#include <sstream>
#include <string>
#include "nova/utils/subsecond.h"


namespace nova { namespace datastores {

    class DatastoreStatusTestsFixture;

    /** Answers the question "what is the status of the Datastore application
     *  on this box?" The answer can be that the application is not installed,
     *  or the state of the application is determined by calling a series of
     *  commands.
     *  This class also handles reporting the status of the datastore to
     *  Trove Conductor (note that the 'send' function is virtual so that
     *  unit tests can change this behavior).
     *  The status is updated whenever the update() method is called, except
     *  if the state is changed to building or restart mode using the
     *  "begin_install" and "begin_restart" methods, in which case update will
     *  not report status (this is to avoid making Trove think there is a
     *  problem because the datastore app is down while Sneaky Pete is
     *  restarting it or something).
     *  These modes are exitted (i.e. functionality of update() returns to
     *  normal) when end_install_or_restart() is called.
     */
    class DatastoreStatus {
        friend class DatastoreStatusTestsFixture;

        public:
            /* NOTE: These *MUST* match the values found in
             * trove.instance.models.ServiceStatuses! */
            enum Status {
                //! BEGIN GENERATED CODE
                BLOCKED = 0x2,  // blocked
                BUILDING = 0x9,  // building
                CRASHED = 0x6,  // crashed
                DELETED = 0x5,  // deleted
                FAILED = 0x8,  // failed to spawn
                FAILED_TIMEOUT_GUESTAGENT = 0x18,  // guestagent error
                NEW = 0x17,  // new
                PAUSED = 0x3,  // paused
                RUNNING = 0x1,  // running
                SHUTDOWN = 0x4,  // shutdown
                UNKNOWN = 0x16  // unknown
                //! END GENERATED CODE
            };

            virtual ~DatastoreStatus();

            /** Called right before the datastore is prepared. */
            void begin_install();

            /* Called before restarting the datastore. */
            void begin_restart();

            /* Called if the datastore fails to install. Notifies Conductor. */
            void end_failed_install();

            /* Called after the datastore is installed or restarted. Determines
             * the real, current status of the app and notifies Conductor. */
            void end_install_or_restart();

            /** Useful for diagnostics and logging. */
            const char * get_current_status_string() const;

            /* Returns true if the application should be installed and
             * an attempt to ascertain its status won't result in nonsense. */
            bool is_installed() const;

            /* Returns true iff the application is running. */
            bool is_running() const;

            /** Returns a readable string for each status enum. */
            static const char * status_name(Status status);

            /** Find and report the status of the app on this machine to
             *  Trove Conductor. */
            void update();

            /** Waits for the given time for the real status to change to the
             *  one specified. Does not update the publicly viewable status
             *  unless "update_conductor" is true. */
            bool wait_for_real_state_to_change_to(Status status, int max_time,
                                                  bool update_conductor=false);

        protected:

            DatastoreStatus(nova::rpc::ResilientSenderPtr sender,
                            bool is_installed);

            /** Gets the status of the app on this machine.
             *  Note: This method produces nonsense (SHUTDOWN) if the app is
             *  not installed or the installation procedure failed. */
            virtual Status determine_actual_status() const = 0;

            /** If true, updates are restricted until the mode is switched
             *  off. */
            bool is_restarting() const;

            /** Notifies Trove Conductor or a status change. */
            virtual void set_status(Status status);

        private:
            DatastoreStatus(DatastoreStatus const &);
            DatastoreStatus & operator = (const DatastoreStatus &);

            bool installed;

            bool restart_mode;

            boost::optional<Status> status;

            boost::mutex status_mutex;

            nova::rpc::ResilientSenderPtr sender;
    };

    typedef boost::shared_ptr<DatastoreStatus> DatastoreStatusPtr;

} }

#endif
