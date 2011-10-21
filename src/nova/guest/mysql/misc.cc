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
 *- MySqlGuestException
 *---------------------------------------------------------------------------*/

MySqlGuestException::MySqlGuestException(MySqlGuestException::Code code) throw()
: code(code) {
}

MySqlGuestException::~MySqlGuestException() throw() {
}

const char * MySqlGuestException::what() const throw() {
    switch(code) {
        case MY_CNF_FILE_NOT_FOUND:
            return "my.cnf file not found.";
        default:
            return "MySqlGuest failure.";
    }
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
