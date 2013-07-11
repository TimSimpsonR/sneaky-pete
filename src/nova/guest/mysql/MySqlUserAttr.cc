#include "nova/guest/mysql/types.h"


using boost::optional;
using std::string;


namespace nova { namespace guest { namespace mysql {


MySqlUserAttr::MySqlUserAttr()
: name(boost::none), host(boost::none), password(boost::none)
{
}

void MySqlUserAttr::set_name(const optional<string> & value) {
    this->name = value;
}

void MySqlUserAttr::set_host(const optional<string> & value) {
    this->host = value;
}

void MySqlUserAttr::set_password(const optional<string> & value) {
    this->password = value;
}


} } } // end namespace
