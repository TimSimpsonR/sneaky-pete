#include "nova/guest/mysql/types.h"


using boost::optional;
using std::string;


namespace nova { namespace guest { namespace mysql {


MySqlUser::MySqlUser()
: name(""), password(boost::none), databases(new MySqlDatabaseList())
{
}

void MySqlUser::set_name(const string & value) {
    this->name = value;
}

void MySqlUser::set_password(const optional<string> & value) {
    this->password = value;
}


} } } // end namespace
