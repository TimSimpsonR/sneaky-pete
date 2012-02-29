#ifndef __NOVA_FLAGS_H
#define __NOVA_FLAGS_H

#include <boost/optional.hpp>
#include <boost/smart_ptr.hpp>
#include <map>
#include <string>

namespace nova { namespace flags {

class FlagException : public std::exception {

        public:
            enum Code {
                DUPLICATE_FLAG_VALUE,
                FILE_NOT_FOUND,
                INVALID_FORMAT,
                KEY_NOT_FOUND,
                NO_EQUAL_SIGN,
                NO_STARTING_DASHES,
                PATTERN_GROUP_NOT_MATCHED,
                PATTERN_NOT_MATCHED
            };

            FlagException(const Code code, const char * line) throw();

            virtual ~FlagException() throw();

            static const char * code_to_string(Code code) throw();

            virtual const char * what() const throw();

            const Code code;

            char msg[256];

            bool use_msg;
};

class FlagMap;
typedef boost::shared_ptr<FlagMap> FlagMapPtr;

/** This is a bumpkin version of Nova flags that can't do fancy things such
 *  as read vaues with spaces in them.  */
class FlagMap {

    public:

        // Adds a line in the form "--name=value"
        void add_from_arg(const char * arg, bool ignore_mismatch=false);

        void add_from_arg(const std::string & arg, bool ignore_mismatch=false);

        // Opens up a file and adds everything.
        void add_from_file(const char * file_path);

        static FlagMapPtr create_from_args(size_t count, char** argv,
                                              bool ignore_mismatch=false);

        static FlagMapPtr create_from_file(const char* argv);

        const char * get(const char * const name);

        const char * get(const char * const name,
                         const char * const default_value);

        const char * get(const char * const name, bool throw_on_missing);



        int get_as_int(const char * const name);

        int get_as_int(const char * const name, int default_value);

        void get_sql_connection(std::string & host, std::string & user,
                                std::string & password, std::string & database);

    private:

        typedef std::map<std::string, std::string> Map;

        Map map;
};


class FlagValues {

    public:

        FlagValues(FlagMapPtr flags);

        bool apt_use_sudo() const;

        const char * apt_self_package_name() const;

        int apt_self_update_time_out() const;

        const char * control_exchange() const;

        const char * db_backend() const;

        const char * guest_ethernet_device() const;

        boost::optional<const char *> host() const;

        boost::optional<int> log_file_max_old_files() const;

        boost::optional<const char *> log_file_path() const;

        boost::optional<size_t> log_file_max_size() const;

        boost::optional<double> log_file_max_time() const;

        bool log_use_std_streams() const;

        /** Time MySQL we'll wait for the MySQL app to change from running to
         *  stopped, or vice-versa. */
        int mysql_state_change_wait_time() const;

        const char * node_availability_zone() const;

        unsigned long nova_db_reconnect_wait_time() const;

        const char * nova_sql_database() const;

        const char * nova_sql_host() const;

        const char * nova_sql_password() const;

        unsigned long nova_sql_reconnect_wait_time() const;

        const char * nova_sql_user() const;

        unsigned long periodic_interval() const;

        boost::optional<int> preset_instance_id() const;

        size_t rabbit_client_memory() const;

        const char * rabbit_host() const;

        const char * rabbit_password() const;

        const int rabbit_port() const;

        unsigned long rabbit_reconnect_wait_time() const;

        const char * rabbit_userid() const;

        unsigned long report_interval() const;

        bool use_syslog() const;

    private:

        FlagMapPtr map;

        std::string _nova_sql_database;

        std::string _nova_sql_host;

        std::string _nova_sql_password;

        std::string _nova_sql_user;

};

} } // end nova::flags

#endif
