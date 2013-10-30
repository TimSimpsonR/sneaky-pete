#ifndef __NOVA_GUEST_MYSQL_MYSQLSTATEMENTS_H
#define __NOVA_GUEST_MYSQL_MYSQLSTATEMENTS_H


#include <boost/foreach.hpp>
#include <boost/format.hpp>
#include <boost/optional.hpp>
#include "nova/guest/mysql/MySqlAdmin.h"
#include <string>
#include "nova/utils/regex.h"
#include "nova/Log.h"

using nova::Log;

namespace nova { namespace guest { namespace mysql {

template<typename Connection>
class Visitor : public boost::static_visitor< > {
public:
    Connection & con;
    std::stringstream & query;

    Visitor(Connection & con, std::stringstream & query)
    :   con(con),
        query(query) {
    }

    void operator()(const boost::blank & value) const {
        query << ";";
    }

    void operator()(const bool & value) const {
        query << " = " << (value ? "1" : "0") << ";";
    }

    void operator()(const int & value) const {
        query << " = " << value << ";";
    }

    void operator()(const double & value) const {
        query << " = " << value << ";";
    }

    void operator()(const std::string & value) const {
        const std::string escaped_string(con.escape_string(value.c_str()));
        query << " = '" << escaped_string << "';";
    }
};

template<typename Connection>
std::string create_global_stmt(
    Connection & con, const MySqlServerAssignments::value_type & assignment) {
    std::stringstream query;
    NOVA_LOG_INFO("create_globals_stmt() iterating over assignments");
    query << "SET GLOBAL " << assignment.first;
    const ServerVariableValue & value = assignment.second;
    ::boost::apply_visitor(Visitor<Connection>(con, query), value);
    return query.str();
}


} } }  // end namespace

#endif // __NOVA_GUEST_MYSQL_MYSQLSTATEMENTS_H
