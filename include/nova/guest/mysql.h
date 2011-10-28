#ifndef __NOVA_GUEST_SQL_GUEST_H
#define __NOVA_GUEST_SQL_GUEST_H

#include "guest.h"

#include <cstdlib>
#include <cstdio>
#include <map>
#include <memory>
#include <string>
#include <syslog.h>
#include <unistd.h>
#include <vector>

#include <boost/optional.hpp>
#include <boost/smart_ptr.hpp>
//#include <mysql_connection.h>
//#include <cppconn/driver.h>
//#include <cppconn/exception.h>
//#include <cppconn/resultset.h>
//#include <cppconn/statement.h>
//#include <cppconn/prepared_statement.h>
#include <json/json.h>
#include <vector>


namespace nova { namespace guest { namespace mysql {

    std::string generate_password();

    class MySqlException : public std::exception {

        public:
            enum Code {
                BIND_RESULT_SET_FAILED,
                COULD_NOT_CONNECT,
                ESCAPE_STRING_BUFFER_TOO_SMALL,
                FIELD_INDEX_OUT_OF_BOUNDS,
                GENERAL,
                GET_QUERY_RESULT_FAILED,
                MY_CNF_FILE_NOT_FOUND,
                NEXT_FETCH_FAILED,
                PARAMETER_INDEX_OUT_OF_BOUNDS,
                PREPARE_BIND_FAILED,
                PREPARE_FAILED,
                QUERY_FAILED,
                QUERY_FETCH_RESULT_FAILED,
                QUERY_RESULT_SET_FINISHED,
                QUERY_RESULT_SET_NOT_STARTED,
                RESULT_INDEX_OUT_OF_BOUNDS,
                RESULT_SET_FINISHED,
                RESULT_SET_LEAK,
                RESULT_SET_NOT_STARTED,
            };

            MySqlException(Code code) throw();

            virtual ~MySqlException() throw();

            static const char * code_to_string(Code code);

            virtual const char * what() const throw();

            const Code code;
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

    class MySqlResultSet;

    typedef std::auto_ptr<MySqlResultSet> MySqlResultSetPtr;

    class MySqlPreparedStatement;

    typedef boost::shared_ptr<MySqlPreparedStatement> MySqlPreparedStatementPtr;

    class MySqlConnection {
        public:
            MySqlConnection(const std::string & uri, const std::string & user,
                            const std::string & password);

            ~MySqlConnection();

            std::string escape_string(const char * original);

            void flush_privileges();

            void grant_all_privileges(const char * username,
                                      const char * host);

            void init();

            MySqlPreparedStatementPtr prepare_statement(const char * text);

            MySqlResultSetPtr query(const char * text);

            //void use_database(const char * database);

        private:
           // sql::Driver *driver;
           // sql::Connection *con;
            void * con;
            const std::string password;
            const std::string uri;
            const std::string user;

            void * get_con();
    };

    class MySqlResultSet {
        public:
            virtual ~MySqlResultSet();

            virtual void close() = 0;

            virtual int get_field_count() const = 0;

            virtual int get_row_count() const = 0;

            virtual boost::optional<std::string> get_string(int index) const = 0;

            virtual bool next() = 0;
    };

    class MySqlPreparedStatement {
        public:
            virtual ~MySqlPreparedStatement();
            virtual void close() = 0;
            virtual MySqlResultSetPtr execute(int result_count = 0) = 0;
            virtual int get_parameter_count() const = 0;
            virtual void set_string(int index, const char * value) = 0;
    };

    typedef boost::shared_ptr<MySqlConnection> MySqlConnectionPtr;

    class MySqlGuest {

        public:
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

            virtual JsonDataPtr handle_message(JsonObjectPtr json);


            typedef nova::JsonObjectPtr (* MethodPtr)(const MySqlGuestPtr &,
                                                      nova::JsonObjectPtr);
            typedef std::map<std::string, MethodPtr> MethodMap;

        private:

            MySqlGuestPtr guest;
            MethodMap methods;
    };

} } }  // end namespace

#endif //__NOVA_GUEST_SQL_GUEST_H
