#ifndef __NOVA_DB_MYSQL_H
#define __NOVA_DB_MYSQL_H


#include <memory>
#include <boost/optional.hpp>
#include <boost/smart_ptr.hpp>
#include <string>
#include <vector>


namespace nova { namespace db { namespace mysql {

    class MySqlException : public std::exception {

        public:
            enum Code {
                BIND_RESULT_SET_FAILED,
                COULD_NOT_CONNECT,
                COULD_NOT_CONVERT_TO_BOOL,
                COULD_NOT_CONVERT_TO_INT,
                ERROR_INITIALIZING_API,
                ESCAPE_STRING_BUFFER_TOO_SMALL,
                FIELD_INDEX_OUT_OF_BOUNDS,
                GENERAL,
                GET_QUERY_RESULT_FAILED,
                MY_CNF_FILE_NOT_FOUND,
                NEXT_FETCH_FAILED,
                NO_PASSWORD_FOR_CREATE_USER,
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
                UNEXPECTED_NULL_FIELD
            };

            MySqlException(Code code) throw();

            virtual ~MySqlException() throw();

            static const char * code_to_string(Code code);

            virtual const char * what() const throw();

            const Code code;
    };

    /* Toss an instance of this in your main, or anywhere else you'll need to
     * use the MySQL API, before any connections are open. */
    class MySqlApiScope {
        public:
            // MySQL allocates some global memory it keeps up with as it runs.
            MySqlApiScope();
            // Call this once you're 100% finished with MySQL or Valgrind
            // will complain about a leak. If you forget it isn't a huge deal.
            ~MySqlApiScope();
    };

    class MySqlResultSet;

    typedef std::auto_ptr<MySqlResultSet> MySqlResultSetPtr;

    class MySqlPreparedStatement;

    typedef boost::shared_ptr<MySqlPreparedStatement> MySqlPreparedStatementPtr;

    class MySqlConnection;

    typedef boost::shared_ptr<MySqlConnection> MySqlConnectionPtr;

    class MySqlConnection {
        public:
            MySqlConnection(const char * uri, const char * user,
                            const char * password);

            /* The user name and password are loaded from the my.cnf file. */
            MySqlConnection(const char * uri);

            virtual ~MySqlConnection();

            void close();

            /* Opens and closes connection. */
            void ensure();

            std::string escape_string(const char * original);

            void flush_privileges();

            static void get_auth_from_config(std::string & user,
                                             std::string & password);

            void grant_all_privileges(const char * username,
                                      const char * host);

            void init();

            MySqlPreparedStatementPtr prepare_statement(const char * text);

            MySqlResultSetPtr query(const char * text);

        private:
            MySqlConnection(const MySqlConnection &);
            MySqlConnection & operator = (const MySqlConnection &);

            void * con;

            void * get_con();

            virtual const char * get_db_name() const;

            std::string password;

            const std::string uri;

            const bool use_mycnf;

            std::string user;
    };

    /** This is just like the above but enforces a default db_name is given.
     *  This is so classes which depend on using a specific database can know
     *  at compile time one is provided. */
    class MySqlConnectionWithDefaultDb : public MySqlConnection {
        public:
            MySqlConnectionWithDefaultDb(const char * uri, const char * user,
                                         const char * password,
                                         const char * db_name);

        private:
            const std::string db_name;

            virtual const char * get_db_name() const;
    };

    typedef boost::shared_ptr<MySqlConnectionWithDefaultDb>
        MySqlConnectionWithDefaultDbPtr;

    class MySqlResultSet {
        public:
            virtual ~MySqlResultSet();

            virtual void close() = 0;

            virtual int get_field_count() const = 0;

            virtual int get_row_count() const = 0;

            boost::optional<bool> get_bool(int index) const;

            boost::optional<int> get_int(int index) const;

            int get_int_non_null(int index) const;

            virtual boost::optional<std::string> get_string(int index) const = 0;

            virtual bool next() = 0;
    };

    class MySqlPreparedStatement {
        public:
            virtual ~MySqlPreparedStatement();
            virtual void close() = 0;
            virtual MySqlResultSetPtr execute(int result_count = 0) = 0;
            virtual int get_parameter_count() const = 0;
            virtual void set_bool(int index, bool value);
            virtual void set_int(int index, int value);
            virtual void set_string(int index, const char * value) = 0;
    };

} } }  // end namespace

#endif //__NOVA_GUEST_SQL_GUEST_H
