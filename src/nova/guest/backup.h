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
#include <boost/utility.hpp>

namespace nova { namespace guest { namespace backup {

    class Backup : boost::noncopyable {
        public:
            Backup(nova::db::mysql::MySqlConnectionWithDefaultDbPtr & infra_db,
                   const int chunk_size,
                   const int segment_max_size,
                   const std::string swift_container,
                   const bool use_gzip,
                   const double time_out);

            ~Backup();


            void run_backup(const std::string & swift_url,
                            const std::string & tenant,
                            const std::string & token,
                            const std::string & backup_id);


        private:
            nova::db::mysql::MySqlConnectionWithDefaultDbPtr infra_db;
            const int chunk_size;
            const int segment_max_size;
            const std::string swift_container;
            const bool use_gzip;
            const std::string swift_url;
            const double time_out;
    };

    /** Represents an instance of a in-flight backup. */
    class BackupRunner : boost::noncopyable {
        public:
            BackupRunner(
                nova::db::mysql::MySqlConnectionWithDefaultDbPtr infra_db,
                const int & chunk_size,
                const int & segment_max_size,
                const std::string & swift_container,
                const std::string & swift_url,
                const double time_out,
                const std::string & tenant,
                const std::string & token,
                const std::string & backup_id);

            ~BackupRunner();

            void run();

        private:
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

            void set_state(const std::string & new_value);
            void update_backup(const std::string & checksum,
                               const std::string & type,
                               const std::string & location);
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
          BackupMessageHandler(Backup & backup);

          virtual nova::JsonDataPtr handle_message(const GuestInput & input);

        private:
          BackupMessageHandler(const BackupMessageHandler &);
          BackupMessageHandler & operator = (const BackupMessageHandler &);

          Backup & backup;
    };

} } }  // end namespace

#endif //__NOVA_GUEST_BACKUP_H
