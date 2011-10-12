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
 *- MySQLUser
 *---------------------------------------------------------------------------*/

MySQLUser::MySQLUser()
: name(""), password(""), databases("")
{
}

void MySQLUser::set_name(const std::string & value) {
    this->name = value;
}

void MySQLUser::set_password(const std::string & value) {
    this->password = value;
}

void MySQLUser::set_databases(const std::string & value) {
    this->databases = value;
}


/**---------------------------------------------------------------------------
 *- MySQLDatabase
 *---------------------------------------------------------------------------*/

MySQLDatabase::MySQLDatabase()
: name(""), collation(""), charset("")
{
}

void MySQLDatabase::set_name(const std::string & value) {
    this->name = value;
}

void MySQLDatabase::set_collation(const std::string & value) {
    this->collation = value;
}

void MySQLDatabase::set_charset(const std::string & value) {
    this->charset = value;
}


} } } // end namespace
