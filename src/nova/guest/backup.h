#ifndef __NOVA_GUEST_BACKUP_H
#define __NOVA_GUEST_BACKUP_H

#include <boost/optional.hpp>
#include "nova/db/mysql.h"
#include <boost/shared_ptr.hpp>
#include "nova/guest/guest.h"
#include "nova/process.h"
#include <map>
#include <string>
#include <curl/curl.h>
#include "nova/utils/swift.h"
#include "nova/utils/threads.h"
#include <boost/utility.hpp>

namespace nova { namespace guest { namespace backup {

    class BackupManager : boost::noncopyable {
        public:
            BackupManager(
                   nova::db::mysql::MySqlConnectionWithDefaultDbPtr & infra_db,
                   nova::utils::JobRunner & runner,
                   const int chunk_size,
                   const int segment_max_size,
                   const std::string swift_container,
                   const bool use_gzip,
                   const double time_out);

            ~BackupManager();

            void run_backup(const std::string & swift_url,
                            const std::string & tenant,
                            const std::string & token,
                            const std::string & backup_id);


        private:
            nova::db::mysql::MySqlConnectionWithDefaultDbPtr infra_db;
            const int chunk_size;
            nova::utils::JobRunner & runner;
            const int segment_max_size;
            const std::string swift_container;
            const bool use_gzip;
            const std::string swift_url;
            const double time_out;
    };


    /** Represents an instance of a in-flight backup. */
    class BackupJob : public nova::utils::Job {
        public:
            BackupJob(
                nova::db::mysql::MySqlConnectionWithDefaultDbPtr infra_db,
                const int & chunk_size,
                const int & segment_max_size,
                const std::string & swift_container,
                const std::string & swift_url,
                const double time_out,
                const std::string & tenant,
                const std::string & token,
                const std::string & backup_id);

            // Copy constructor is designed mainly so we can pass instances
            // from one thread to another.
            BackupJob(const BackupJob & other);

            ~BackupJob();

            virtual void operator()();

            virtual nova::utils::Job * clone() const;

        private:
            // Don't allow this, despite the copy constructor above.
            BackupJob & operator=(const BackupJob & rhs);

            struct DbInfo {
                const std::string & checksum;
                const std::string & type;
                const std::string & location;
            };

            const std::string backup_id;
            nova::db::mysql::MySqlConnectionWithDefaultDbPtr infra_db;
            const int chunk_size;
            const int segment_max_size;
            const std::string swift_container;
            const std::string swift_url;
            const std::string tenant;
            const double time_out;
            const std::string token;

            void dump();

            boost::optional<std::string> get_state();

            void update_db(
                const std::string & new_value,
                const boost::optional<const DbInfo> & extra_info = boost::none
            );
    };


    class BackupException : public std::exception {

        public:
            enum Code {
                INVALID_STATE
            };

            BackupException(const Code code) throw();

            virtual ~BackupException() throw();

            virtual const char * what() const throw();

            const Code code;
    };


    class BackupMessageHandler : public MessageHandler {

        public:
          BackupMessageHandler(BackupManager & backup_manager);

          virtual nova::JsonDataPtr handle_message(const GuestInput & input);

        private:
          BackupMessageHandler(const BackupMessageHandler &);
          BackupMessageHandler & operator = (const BackupMessageHandler &);

          BackupManager & backup_manager;
    };

} } }  // end namespace

#endif //__NOVA_GUEST_BACKUP_H
