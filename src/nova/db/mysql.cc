#include "nova/db/mysql.h"
#include "nova/Log.h"
#include <boost/format.hpp>
#include <fstream>
#include <iostream>
#include <boost/lexical_cast.hpp>
#include <mysql/mysql.h>
#include <regex.h>  //TODO(tim.simpson): Use the Regex classes here.
#include <string.h>

using boost::format;
using boost::none;
using boost::optional;
using nova::Log;
using namespace std;


namespace nova { namespace db { namespace mysql {

namespace {

    inline MYSQL * mysql_con(void * con) {
        return (MYSQL *) con;
    }

    void append_match_to_string(string & str, regex_t regex,
                                const char * line) {
        // 2 matches, the 2nd is the ()
        int num_matches = 2;
        regmatch_t matches[num_matches];
        if (regexec(&regex, line, num_matches, matches, 0) == 0) {
            const regoff_t & start_index = matches[num_matches-1].rm_so;
            const regoff_t & end_index = matches[num_matches-1].rm_eo;
            const char* begin_of_new_string = line + start_index;
            const regoff_t match_size = end_index - start_index;
            if (match_size > 0) {
                str.append(begin_of_new_string, match_size);
            }
        }
    }

    //TODO(tim.simpson): Get rid of this class and IntParameterBuffer, turns out they aren't needed.
    struct ParameterBuffer {

        virtual void bind(MYSQL_BIND & bind) = 0;

        virtual void set(const char * new_value) = 0;

        virtual optional<string> to_string() = 0;
    };

    // This may need to go to the trash.
    struct IntParameterBuffer {
        int buffer;
        my_bool error;
        unsigned long length;
        my_bool is_null;

        virtual void bind(MYSQL_BIND & bind) {
            bind.buffer_type= MYSQL_TYPE_LONG;
            bind.buffer= (char *)&buffer;
            //bind.buffer_length = 256; // Not specified?
            bind.is_null = &is_null;
            bind.length= &length;
            bind.error= &error;
        }

        virtual void set(const int & new_value) {
            buffer = new_value;
            is_null = 0;
            error = 0;
        }

        virtual optional<string> to_string() {
            if (is_null) {
                return boost::none;
            } else {
                string value = str(format("%s") % buffer);
                optional<string> rtn(value);
                return rtn;
            }
        }
    };

    struct StringParameterBuffer {
        char buffer[256];
        my_bool error;
        unsigned long length;
        my_bool is_null;

        virtual void bind(MYSQL_BIND & bind) {
            bind.buffer_type = MYSQL_TYPE_STRING;
            bind.buffer = buffer;
            bind.buffer_length = 256;
            bind.is_null = &is_null;
            bind.length = &length;
            bind.error = &error;
        }

        virtual void set(const char * new_value) {
            length = strnlen(new_value, 256);
            strncpy(buffer, new_value, length);
            is_null = 0;
            error = 0;
        }

        virtual optional<string> to_string() {
            if (is_null) {
                return boost::none;
            } else {
                string value(buffer, length);
                optional<string> rtn(value);
                return rtn;
            }
        }
    };
}  // end anonymous namespace


/**---------------------------------------------------------------------------
 *- MySqlException
 *---------------------------------------------------------------------------*/

MySqlException::MySqlException(MySqlException::Code code) throw()
: code(code) {
}

MySqlException::~MySqlException() throw() {
}

const char *  MySqlException::code_to_string(Code code) {
    switch(code) {
        case BIND_RESULT_SET_FAILED:
            return "Binding result set failed.";
        case COULD_NOT_CONNECT:
            return "Could not connect to database.";
        case COULD_NOT_CONVERT_TO_BOOL:
            return "Could not convert result set field to boolean.";
        case COULD_NOT_CONVERT_TO_INT:
            return "Could not convert result set field to integer.";
        case ERROR_INITIALIZING_API:
            return "An error occurred initializing the MySQL API!";
        case ESCAPE_STRING_BUFFER_TOO_SMALL:
            return "Escape string buffer was too small.";
        case FIELD_INDEX_OUT_OF_BOUNDS:
            return "Query result field index out of bounds.";
        case GET_QUERY_RESULT_FAILED:
            return "Failed to get the result of the query.";
        case MY_CNF_FILE_NOT_FOUND:
            return "my.cnf file not found.";
        case NEXT_FETCH_FAILED:
            return "Error fetching next result.";
        case NO_PASSWORD_FOR_CREATE_USER:
            return "Can't create user because no password was specified.";
        case PARAMETER_INDEX_OUT_OF_BOUNDS:
            return "Parameter index out of bounds.";
        case PREPARE_BIND_FAILED:
            return "Prepare statement bind failed.";
        case PREPARE_FAILED:
            return "An error occurred creating prepared statement.";
        case PREPARE_STATEMENT_FAILED:
            return "Failed prepare_statement call.";
        case QUERY_FAILED:
            return "Query failed.";
        case QUERY_FETCH_RESULT_FAILED:
            return "Failed to fetch a query result.";
        case QUERY_RESULT_SET_FINISHED:
            return "The query result set was already iterated.";
        case QUERY_RESULT_SET_NOT_STARTED:
            return "The query result set has not begun. Call next.";
        case RESULT_INDEX_OUT_OF_BOUNDS:
            return "Result index out of bounds.";
        case RESULT_SET_FINISHED:
            return "Attempt to grab result from a result set that has finished.";
        case RESULT_SET_LEAK:
            return "A result set was not freed. Close it before closing statement.";
        case RESULT_SET_NOT_STARTED:
            return "Attempt to grab result without calling the next method.";
        case UNEXPECTED_NULL_FIELD:
            return "A result set field wasn't expected to be null but was.";
        default:
            return "MySqlGuest failure.";
    }
}

const char * MySqlException::what() const throw() {
    return code_to_string(code);
}


/**---------------------------------------------------------------------------
 *- MySqlApiScope
 *---------------------------------------------------------------------------*/

MySqlApiScope::MySqlApiScope() {
    if (mysql_library_init(0, NULL, NULL) != 0) {
        throw MySqlException(MySqlException::ERROR_INITIALIZING_API);
    }
}

MySqlApiScope::~MySqlApiScope() {
    mysql_library_end();
}


/**---------------------------------------------------------------------------
 *- MySqlResultSet
 *---------------------------------------------------------------------------*/

MySqlResultSet::~MySqlResultSet() {

}

optional<bool> MySqlResultSet::get_bool(int index) const {
    optional<string> value = get_string(index);
    if (!value) {
        return boost::none;
    }
    try {
        bool b_value = boost::lexical_cast<bool>(value.get());
        return optional<bool>(b_value);
    } catch(const boost::bad_lexical_cast & blc) {
        NOVA_LOG_ERROR2("Could not convert the result field at index %d with "
                        "value %s to a bool.", index, value.get().c_str());
        throw MySqlException(MySqlException::COULD_NOT_CONVERT_TO_BOOL);
    }
}

optional<int> MySqlResultSet::get_int(int index) const {
    optional<string> value = get_string(index);
    if (!value) {
        return boost::none;
    }
    try {
        int i_value = boost::lexical_cast<int>(value.get());
        return optional<int>(i_value);
    } catch(const boost::bad_lexical_cast & blc) {
        NOVA_LOG_ERROR2("Could not convert the result field at index %d with "
                        "value \"%s\" to an int.", index, value.get().c_str());
        throw MySqlException(MySqlException::COULD_NOT_CONVERT_TO_INT);
    }
}

int MySqlResultSet::get_int_non_null(int index) const {
    optional<string> value = get_string(index);
    if (!value) {
        throw MySqlException(MySqlException::UNEXPECTED_NULL_FIELD);
    }
    try {
        int i_value = boost::lexical_cast<int>(value.get());
        return i_value;
    } catch(const boost::bad_lexical_cast & blc) {
        throw MySqlException(MySqlException::COULD_NOT_CONVERT_TO_INT);
    }
}


/**---------------------------------------------------------------------------
 *- MySqlQueryResultSet
 *---------------------------------------------------------------------------*/
class MySqlQueryResultSet : public MySqlResultSet {

public:
    MySqlQueryResultSet(MYSQL * con)
    : con(con), current_row(0), field_count(0), finished(false), result(0),
      row_count(0), started(false)
    {
        result = mysql_store_result(con);
        // The docs imply this is safe to call even if an error occurs.
        if (result == 0) {
            if (mysql_errno(con) == 0) {
                // Means there was no result set. This is OK.
                started = true;
                finished = true;
            } else {
                NOVA_LOG_ERROR2("Error getting store result from query: %s",
                                mysql_error(con));
                throw MySqlException(MySqlException::GET_QUERY_RESULT_FAILED);
            }
        } else {
            field_count = mysql_num_fields(result);
        }
    }

    virtual ~MySqlQueryResultSet() {
        close();
    }

    virtual void close() {
        if (result != 0) {
            finished = true;
            mysql_free_result(result);
            result = 0;
        }
    }

    virtual int get_field_count() const {
        return field_count;
    }

    virtual optional<string> get_string(int index) const {
        if (index < 0 || index >= field_count) {
            throw MySqlException(MySqlException::FIELD_INDEX_OUT_OF_BOUNDS);
        }
        if (finished) {
            throw MySqlException(MySqlException::QUERY_RESULT_SET_FINISHED);
        }
        if (!started) {
            throw MySqlException(MySqlException::QUERY_RESULT_SET_NOT_STARTED);
        }
        if (current_row[index] == 0) {
            return boost::none;
        } else {
            optional<string> rtn(string(current_row[index]));
            return rtn;
        }
    }

    virtual bool next() {
        if (finished) {
            return false;
        }
        current_row = mysql_fetch_row(result);
        started = true;
        if (current_row != 0) {
            row_count ++;
            return true;
        } else {
            if (mysql_errno(con) == 0) {
                close();
                return false;
            } else {
                NOVA_LOG_ERROR2("Fetch next row failed:%s", mysql_error(con));
                throw MySqlException(MySqlException::QUERY_FETCH_RESULT_FAILED);
            }
        }
    }

    virtual int get_row_count() const {
        return row_count;
    }

private:
    MYSQL * con;
    MYSQL_ROW current_row;
    int field_count;
    bool finished;
    MYSQL_RES * result;
    int row_count;
    bool started;
};


/**---------------------------------------------------------------------------
 *- MySqlPreparedStatement
 *---------------------------------------------------------------------------*/

MySqlPreparedStatement::~MySqlPreparedStatement() {

}

void MySqlPreparedStatement::set_bool(int index, bool value) {
    set_string(index, value ? "true" : "false");
}

void MySqlPreparedStatement::set_int(int index, int value) {
    string string_value = str(format("%d") % value);
    set_string(index, string_value.c_str());
}

/**---------------------------------------------------------------------------
 *- MySqlPreparedStatementImpl
 *---------------------------------------------------------------------------*/

class MySqlPreparedStatementImpl : public MySqlPreparedStatement {

public:

    MySqlPreparedStatementImpl(MYSQL * con, const char * statement)
        : bind(0), con(con), parameter_buffer(0), parameter_count(-1), stmt(0)
    {
        stmt = mysql_stmt_init(con);
        if (stmt == 0) {
            NOVA_LOG_ERROR("No memory to make statement?");
            throw MySqlException(MySqlException::PREPARE_FAILED);
        }
        size_t statement_length = strnlen(statement, 1028);
        if (mysql_stmt_prepare(stmt, statement, statement_length) != 0) {
            NOVA_LOG_ERROR2("An error occurred preparing statement:%s",
                            mysql_stmt_error(stmt));
            throw MySqlException(MySqlException::PREPARE_FAILED);
        }
        parameter_count = mysql_stmt_param_count(stmt);
        bind = new MYSQL_BIND[parameter_count];
        memset(bind, 0, sizeof(bind));
        parameter_buffer = new StringParameterBuffer[parameter_count];
        for (size_t index = 0; index < (size_t) parameter_count; index ++) {
            // Initialize parameter to empty string.
            parameter_buffer[index].set("");
            // The bind buffer stuff just points to memory elsewhere.
            parameter_buffer[index].bind(bind[index]);
        }
        if (mysql_stmt_bind_param(stmt, bind) != 0) {
            NOVA_LOG_ERROR2("Prepared statement bind parm failed: %s",
                            mysql_stmt_error(stmt));
            throw MySqlException(MySqlException::PREPARE_BIND_FAILED);
        }
    }

    ~MySqlPreparedStatementImpl() {
        close();
    }

    virtual void close() {
        if (bind != 0 ) {
            delete[] bind;
            bind = 0;
        }
        if (parameter_buffer != 0) {
            delete[] parameter_buffer;
            parameter_buffer = 0;
        }
        if (stmt != 0) {
            mysql_stmt_close(stmt);
            stmt = 0;
        }
    }

    virtual void execute(int result_count) {
        if (mysql_stmt_execute(stmt) != 0) {
            NOVA_LOG_ERROR2("execute failed: %s", mysql_stmt_error(stmt));
            throw MySqlException(MySqlException::PREPARE_STATEMENT_FAILED);
        }
    }

    virtual int get_parameter_count() const {
        return parameter_count;
    }

    virtual void set_string(int index, const char * value) {
        if (index < 0 || index >= parameter_count) {
            throw MySqlException(MySqlException::PARAMETER_INDEX_OUT_OF_BOUNDS);
        }
        parameter_buffer[index].set(value);
    }

private:
    MYSQL_BIND * bind;
    MYSQL * con;
    StringParameterBuffer * parameter_buffer;
    int parameter_count;
    MYSQL_STMT * stmt;
};


/**---------------------------------------------------------------------------
 *- MySqlConnection
*---------------------------------------------------------------------------*/

MySqlConnection::MySqlConnection(const char * uri,
                                 const char * user,
                                 const char * password)
: con(0), password(password), uri(uri), use_mycnf(false),
  user(user) {
}

MySqlConnection::MySqlConnection(const char * uri)
: con(0), password(""), uri(uri), use_mycnf(true),
  user("") {
}

MySqlConnection::~MySqlConnection() {
    this->close();
}

void MySqlConnection::close() {
    if (mysql_con(con) != 0) {
        mysql_close(mysql_con(con));
        con = 0;
    }
}

void MySqlConnection::ensure() {
    // Ensure a fresh connection.
    const int result = mysql_con(con) == 0 ? -1 : mysql_ping(mysql_con(con));
    if (result != 0) {
        close();
        init();
    }
}

std::string MySqlConnection::escape_string(const char * original) {
    size_t len = strnlen(original, 1025);
    if (len > 1024) {
        throw MySqlException(MySqlException::ESCAPE_STRING_BUFFER_TOO_SMALL);
    }
    char * buffer = new char[len * 2 + 1];
    mysql_real_escape_string(mysql_con(get_con()), buffer, original, len);
    string rtn = buffer;
    delete[] buffer;
    return rtn;
}

void MySqlConnection::flush_privileges() {
    MySqlPreparedStatementPtr stmt = prepare_statement(
        "FLUSH PRIVILEGES;");
    stmt->execute();
    stmt->close();
}

void * MySqlConnection::get_con() {
    if (mysql_con(con) == 0) {
        init();
    }
    return con;
}

const char * MySqlConnection::get_db_name() const {
    return NULL;
}

void MySqlConnection::get_auth_from_config(string & user, string & password) {
    const char *pattern = "^\\w+\\s*=\\s*['\"]?(.[^'\"]*)['\"]?\\s*$";
    regex_t regex;
    regcomp(&regex, pattern, REG_EXTENDED);

    // TODO(tim.simpson): This should be the normal my.cnf, but we can't
    // read it from there... yet.
    ifstream my_cnf("/var/lib/nova/my.cnf");
    if (!my_cnf.is_open()) {
        throw MySqlException(MySqlException::MY_CNF_FILE_NOT_FOUND);
    }
    std::string line;
    bool is_in_client = false;
    while(my_cnf.good()) {
        getline(my_cnf, line);
        if (strstr(line.c_str(), "[client]")) {
            is_in_client = true;
        }
        if (strstr(line.c_str(), "[mysqld]")) {
            is_in_client = false;
        }
        // Be careful - end index is non-inclusive.
        if (is_in_client && strstr(line.c_str(), "user")) {
            append_match_to_string(user, regex, line.c_str());
        }
        if (is_in_client && strstr(line.c_str(), "password")) {
            append_match_to_string(password, regex, line.c_str());
        }
    }
    my_cnf.close();

    regfree(&regex);
}

void MySqlConnection::grant_all_privileges(const char * username,
                                      const char * host) {
    //TODO(tim.simpson): Fix this to use parameters.
    string text = str(format(
        "GRANT ALL PRIVILEGES ON *.* TO '%s'@'%s' WITH GRANT OPTION;")
        % escape_string(username) % escape_string(host));
    MySqlPreparedStatementPtr stmt = prepare_statement(text.c_str());
    stmt->execute();
    stmt->close();
}

void MySqlConnection::revoke_privileges(const char * username,
                                        const char * host,
                                        const char * privs) {
    string text = str(format("REVOKE %s ON *.* FROM '%s'@'%s';")
                      % escape_string(privs) % escape_string(username)
                      % escape_string(host));
    MySqlPreparedStatementPtr stmt = prepare_statement(text.c_str());
    stmt->execute();
    stmt->close();
}

void MySqlConnection::init() {
    if (con != 0) {
        throw std::exception();
    }

    if (use_mycnf) {
        get_auth_from_config(user, password);
    }

    con = mysql_init(NULL);
    if (con == NULL) {
        NOVA_LOG_ERROR2("Error %u: %s\n", mysql_errno(0), mysql_error(0));
        throw MySqlException(MySqlException::GENERAL);
    }

    // my_bool reconnect = 1;
    // mysql_options(mysql_con(con), MYSQL_OPT_RECONNECT, &reconnect);

    if (mysql_real_connect(mysql_con(con), uri.c_str(), user.c_str(),
                           password.c_str(),
                           get_db_name(), /*port*/ 0, /* socket */NULL,
                           /* Flag */0) == NULL) {
        NOVA_LOG_ERROR2("Error %u: %s\n", mysql_errno(mysql_con(con)),
                        mysql_error(mysql_con(con)));
        throw MySqlException(MySqlException::COULD_NOT_CONNECT);
    }

    // if (con != 0) {
    //     throw std::exception();
    // }
    // driver = get_driver_instance();
    // log.info2("Connecting to %s with username:%s, password:%s",
    //           uri.c_str(), user.c_str(), password.c_str());
    // con = driver->connect(uri, user, password);
}

MySqlPreparedStatementPtr MySqlConnection::prepare_statement(
    const char * text)
{
    MySqlPreparedStatementPtr stmt(
        new MySqlPreparedStatementImpl(mysql_con(get_con()), text));
    return stmt;
}

MySqlResultSetPtr MySqlConnection::query(const char * text) {
    if (mysql_query(mysql_con(get_con()), text) != 0) {
        NOVA_LOG_ERROR2("Query failed:%s", mysql_error(mysql_con(con)));
        throw MySqlException(MySqlException::QUERY_FAILED);
    }
    MySqlResultSetPtr rtn(new MySqlQueryResultSet(mysql_con(get_con())));
    return rtn;
}


/**---------------------------------------------------------------------------
 *- MySqlConnectionWithDefaultDb
*---------------------------------------------------------------------------*/

MySqlConnectionWithDefaultDb::MySqlConnectionWithDefaultDb(
    const char * uri,
    const char * user,
    const char * password,
    const char * db_name)
: MySqlConnection(uri, user, password), db_name(db_name) {
}

const char * MySqlConnectionWithDefaultDb::get_db_name() const {
    return db_name.c_str();
}

} } } // nova::guest::mysql
