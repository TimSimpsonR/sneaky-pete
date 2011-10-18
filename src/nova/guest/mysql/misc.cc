#include "nova/guest/guest.h"
#include "nova/guest/mysql.h"


using namespace std;


namespace nova { namespace guest { namespace mysql {


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
: name(""), password(""), databases("")
{
}

void MySqlUser::set_name(const std::string & value) {
    this->name = value;
}

void MySqlUser::set_password(const std::string & value) {
    this->password = value;
}

void MySqlUser::set_databases(const std::string & value) {
    this->databases = value;
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
