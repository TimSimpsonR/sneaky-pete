#include "nova/guest/guest.h"
#include "nova/guest/mysql.h"
#include <uuid/uuid.h>

using namespace std;


namespace nova { namespace guest { namespace mysql {


string generate_password() {
    uuid_t id;
    uuid_generate(id);
    char *buf = new char[37];
    uuid_unparse(id, buf);
    uuid_clear(id);
    return string(buf);
}


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
        case PARAMETER_INDEX_OUT_OF_BOUNDS:
            return "Parameter index out of bounds.";
        case PREPARE_BIND_FAILED:
            return "Prepare statement bind failed.";
        case PREPARE_FAILED:
            return "An error occurred creating prepared statement.";
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
        default:
            return "MySqlGuest failure.";
    }
}

const char * MySqlException::what() const throw() {
    return code_to_string(code);
}

/**---------------------------------------------------------------------------
 *- MySqlUser
 *---------------------------------------------------------------------------*/

MySqlUser::MySqlUser()
: name(""), password(""), databases(new MySqlDatabaseList())
{
}

void MySqlUser::set_name(const std::string & value) {
    this->name = value;
}

void MySqlUser::set_password(const std::string & value) {
    this->password = value;
}


/**---------------------------------------------------------------------------
 *- MySqlDatabase
 *---------------------------------------------------------------------------*/

MySqlDatabase::MySqlDatabase()
: character_set(""), collation(""), name("")
{
}

void MySqlDatabase::set_name(const std::string & value) {
    this->name = value;
}

void MySqlDatabase::set_collation(const std::string & value) {
    this->collation = value;
}

void MySqlDatabase::set_character_set(const std::string & value) {
    this->character_set = value;
}


} } } // end namespace
