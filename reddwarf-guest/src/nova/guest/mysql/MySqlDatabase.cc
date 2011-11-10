#include "nova/guest/guest.h"
#include "nova/guest/mysql/types.h"


using std::string;


namespace nova { namespace guest { namespace mysql {

MySqlDatabase::MySqlDatabase()
: character_set(""), collation(""), name("")
{
}

const char * MySqlDatabase::default_character_set() {
    return "utf8";
}

const char * MySqlDatabase::default_collation() {
    return "utf8_general_ci";
}

void MySqlDatabase::set_name(const string & value) {
    this->name = value;
}

void MySqlDatabase::set_collation(const string & value) {
    this->collation = value;
}

void MySqlDatabase::set_character_set(const string & value) {
    this->character_set = value;
}

} } } // end namespace
