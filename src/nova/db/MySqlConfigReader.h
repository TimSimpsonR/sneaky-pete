#ifndef __NOVA_DB_MYSQLCONFIGREADER_H
#define __NOVA_DB_MYSQLCONFIGREADER_H


#include <boost/optional.hpp>
#include <string>
#include "nova/utils/regex.h"


namespace nova { namespace db { namespace mysql {


struct MySqlConfigCreds {
    std::string user;
    std::string password;
};


template<typename StreamType>
boost::optional<MySqlConfigCreds> get_creds_from_file(
    StreamType & stream) {
    using namespace boost;
    using namespace std;
    using nova::utils::Regex;

    Regex header("^\\[\\s*(\\w+)\\s*\\]\\s*$");
    Regex assignment("^(\\w+)\\s*=\\s*['\"]?(.[^'\"]*)['\"]?\\s*$");

    optional<string> user_name;
    optional<string> password;
    string line;
    bool is_in_client = false;
    while(stream.good()) {
        getline(stream, line);
        auto header_matches = header.match(line.c_str());
        if (header_matches && header_matches->exists_at(1)) {
            is_in_client = (header_matches->get(1) == "client");
        } else if (is_in_client) {
            auto assignment_matches = assignment.match(line.c_str());
            if (assignment_matches && assignment_matches->exists_at(2)) {
                if (assignment_matches->get(1) == "user") {
                    user_name = optional<string>(assignment_matches->get(2));
                } else if (assignment_matches->get(1) == "password") {
                    password = optional<string>(assignment_matches->get(2));
                }
            }
        }
    }
    if (user_name && password) {
        MySqlConfigCreds creds = { user_name.get(), password.get() };
        return optional<MySqlConfigCreds>(creds);
    } else {
        return boost::none;
    }
}


} } }  // end namespace

#endif // __NOVA_DB_MYSQLCONFIGREADER_H
