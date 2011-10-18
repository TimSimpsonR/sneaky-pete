#ifndef __NOVA_GUEST_SQL_GUEST_H
#define __NOVA_GUEST_SQL_GUEST_H

#include "guest.h"

#include <cstdlib>
#include <cstdio>
#include <memory>
#include <string>
#include <syslog.h>
#include <unistd.h>
#include <vector>

#include <boost/smart_ptr.hpp>
#include <mysql_connection.h>
#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>
#include <cppconn/prepared_statement.h>
#include <json/json.h>
#include <vector>


namespace nova { namespace guest { namespace mysql {

    class MySqlGuestException : public std::exception {

        public:
            enum Code {
                GENERAL,
                MY_CNF_FILE_NOT_FOUND,
            };

            MySqlGuestException(Code code) throw();

            virtual ~MySqlGuestException() throw();

            virtual const char * what() const throw();

        private:

            Code code;
    };


    class MySqlDatabase {

        public:
            MySqlDatabase();

            inline const std::string & get_character_set() const {
                return character_set;
            }

            inline const std::string & get_collation() const {
                return collation;
            }

            inline const std::string & get_name() const {
                return name;
            }

            void set_character_set(const std::string & value);

            void set_collation(const std::string & value);

            void set_name(const std::string & value);

        private:
            std::string character_set;
            std::string collation;
            std::string name;
    };

    typedef boost::shared_ptr<MySqlDatabase> MySqlDatabasePtr;
    typedef std::vector<MySqlDatabasePtr> MySqlDatabaseList;
    typedef boost::shared_ptr<MySqlDatabaseList> MySqlDatabaseListPtr;


    class MySqlUser {
    public:
        MySqlUser();

        inline const std::string & get_name() const {
            return name;
        }

        inline const std::string & get_password() const {
            return password;
        }

        inline const std::string & get_databases() const {
            return databases;
        }

        void set_name(const std::string & value);

        void set_password(const std::string & value);

        void set_databases(const std::string & value);

    private:
        std::string name;
        std::string password;
        std::string databases;
    };


    typedef boost::shared_ptr<MySqlUser> MySqlUserPtr;
    typedef std::vector<MySqlUserPtr> MySqlUserList;
    typedef boost::shared_ptr<MySqlUserList> MySqlUserListPtr;

    class MySqlGuest {
        //TODO(tim.simpson): The constructor and "prepare_statement" call could
        // extracted into a general use MySql class used by this one.

        public:
            typedef std::auto_ptr<sql::PreparedStatement> PreparedStatementPtr;

            MySqlGuest(const std::string & uri);
            MySqlGuest(const MySqlGuest & other);
            ~MySqlGuest();

            void create_database(MySqlDatabaseListPtr databases);

            void create_user(MySqlUserPtr);

            void create_users(MySqlUserListPtr);

            void delete_database(const std::string & database_name);

            void delete_user(const std::string & username);

            // The original method always returns True, but I don't know why.
            void disable_root();

            MySqlUserPtr enable_root();

            void flush_privileges();

            static std::string generate_password();

            void grand_all_privileges(const char * username,
                                      const char * host);

            MySqlDatabaseListPtr list_databases();

            MySqlUserListPtr list_users();

            bool is_root_enabled();

            PreparedStatementPtr prepare_statement(const char * text);

            void set_password(const char * username, const char * password);

        private:

            sql::Driver *driver;
            sql::Connection *con;

    };

    typedef boost::shared_ptr<MySqlGuest> MySqlGuestPtr;

    class MySqlMessageHandler : public MessageHandler {

        public:
            MySqlMessageHandler(MySqlGuestPtr guest);
            MySqlMessageHandler(const MySqlMessageHandler & other);

            virtual JsonObjectPtr handle_message(JsonObjectPtr json);


            typedef nova::JsonObjectPtr (* MethodPtr)(const MySqlGuestPtr &,
                                                      nova::JsonObjectPtr);
            typedef std::map<std::string, MethodPtr> MethodMap;

        private:

            MySqlGuestPtr guest;
            MethodMap methods;
    };

} } }  // end namespace

#endif //__NOVA_GUEST_SQL_GUEST_H
