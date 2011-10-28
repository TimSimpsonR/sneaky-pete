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

    std::string generate_password();

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

        inline MySqlDatabaseListPtr get_databases() {
            return databases;
        }

        void set_name(const std::string & value);

        void set_password(const std::string & value);

    private:
        std::string name;
        std::string password;
        MySqlDatabaseListPtr databases;
    };


    typedef boost::shared_ptr<MySqlUser> MySqlUserPtr;
    typedef std::vector<MySqlUserPtr> MySqlUserList;
    typedef boost::shared_ptr<MySqlUserList> MySqlUserListPtr;

    class MySqlConnection {
        public:
            typedef std::auto_ptr<sql::PreparedStatement> PreparedStatementPtr;

            MySqlConnection(const std::string & uri, const std::string & user,
                            const std::string & password);

            ~MySqlConnection();

            std::string escape_string(const char * original);

            void flush_privileges();

            void grant_all_privileges(const char * username,
                                      const char * host);

            void init();

            PreparedStatementPtr prepare_statement(const char * text);

            void use_database(const char * database);

        private:
            sql::Driver *driver;
            sql::Connection *con;
            std::string database;
            const std::string password;
            const std::string uri;
            const std::string user;

            sql::Connection * get_con();
    };

    typedef boost::shared_ptr<MySqlConnection> MySqlConnectionPtr;

    class MySqlGuest {

        public:
            typedef MySqlConnection::PreparedStatementPtr PreparedStatementPtr;

            MySqlGuest(const std::string & uri);
            MySqlGuest(const std::string & uri, const std::string & user,
                       const std::string & password);
            ~MySqlGuest();

            void create_database(MySqlDatabaseListPtr databases);

            void create_user(MySqlUserPtr, const char * host="%");

            void create_users(MySqlUserListPtr);

            void delete_database(const std::string & database_name);

            void delete_user(const std::string & username);

            MySqlUserPtr enable_root();

            inline MySqlConnectionPtr get_connection() {
                return con;
            }

            MySqlDatabaseListPtr list_databases();

            MySqlUserListPtr list_users();

            bool is_root_enabled();

            void set_password(const char * username, const char * password);

        private:
            MySqlGuest(const MySqlGuest & other);

            MySqlConnectionPtr con;

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
