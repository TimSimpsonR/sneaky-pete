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
                MY_CNF_FILE_NOT_FOUND
            };

            MySqlGuestException(Code code) throw();

            virtual ~MySqlGuestException() throw();

            virtual const char * what() const throw();

        private:

            Code code;
    };


    class MySQLUser {
    public:
        MySQLUser();

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

    class MySQLDatabase {
    public:
        MySQLDatabase();

        inline const std::string & get_name() const {
            return name;
        }

        inline const std::string & get_collation() const {
            return collation;
        }

        inline const std::string & get_charset() const {
            return charset;
        }

        void set_name(const std::string & value);

        void set_collation(const std::string & value);

        void set_charset(const std::string & value);

    private:
        std::string name;
        std::string collation;
        std::string charset;
    };

    typedef boost::shared_ptr<MySQLUser> MySQLUserPtr;
    typedef std::vector<MySQLUserPtr> MySQLUserList;
    typedef boost::shared_ptr<MySQLUserList> MySQLUserListPtr;

    typedef boost::shared_ptr<MySQLDatabase> MySQLDatabasePtr;
    typedef std::vector<MySQLDatabasePtr> MySQLDatabaseList;
    typedef boost::shared_ptr<MySQLDatabaseList> MySQLDatabaseListPtr;

    class MySqlGuest {

        public:
            MySqlGuest(const std::string & uri);
            MySqlGuest(const MySqlGuest & other);
            ~MySqlGuest();

            std::string create_user(const std::string & username,
                                    const std::string & password,
                                    const std::string & host);
            MySQLUserListPtr list_users();
            std::string delete_user(const std::string & username);
            std::string create_database(const std::string & database_name,
                                        const std::string & character_set,
                                        const std::string & collate);
            MySQLDatabaseListPtr list_databases();
            std::string delete_database(const std::string & database_name);
            std::string enable_root();
            std::string disable_root();
            bool is_root_enabled();

        private:
            typedef std::auto_ptr<sql::PreparedStatement> PreparedStatementPtr;

            sql::Driver *driver;
            sql::Connection *con;

            PreparedStatementPtr prepare_statement(const char * text);
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
