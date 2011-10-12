#include "nova/guest/guest.h"
#include "nova/Log.h"
#include <boost/foreach.hpp>
#include "nova/guest/mysql.h"
#include <sstream>


using nova::Log;
using nova::JsonObject;
using nova::JsonObjectPtr;
using namespace std;



namespace nova { namespace guest { namespace mysql {

namespace {
    Log log;
}

/**---------------------------------------------------------------------------
 *- MySqlMessageHandler
 *---------------------------------------------------------------------------*/

#define STR_VALUE(arg)  #arg
#define METHOD_NAME(name) STR_VALUE(name)
#define REGISTER(name) { METHOD_NAME(name), & name }
#define JSON_METHOD(name) JsonObjectPtr \
    name(const MySqlGuestPtr & guest, JsonObjectPtr args)

namespace {

    JSON_METHOD(create_database) {
        log.info2("guest create_database"); //", guest->create_database().c_str());
        JsonObjectPtr rtn(new JsonObject("{}"));
        return rtn;
    }

    JSON_METHOD(create_user) {
        string guest_return = guest->create_user("username", "password", "%");
        log.info2("guest call %s", guest_return.c_str());
        JsonObjectPtr rtn(new JsonObject("{}"));
        return rtn;
    }

    JSON_METHOD(list_users) {
        std::stringstream user_xml;
        MySQLUserListPtr users = guest->list_users();
        user_xml << "[";
        BOOST_FOREACH(MySQLUserPtr & user, *users) {
            user_xml << "{'name':'" << user->get_name()
                     << "'},";
        }
        user_xml << "]";
        log.info2("guest call %s", user_xml.str().c_str());
        JsonObjectPtr rtn(new JsonObject("{}"));
        return rtn;
    }

    JSON_METHOD(delete_user) {
        string guest_return = guest->delete_user("username");
        log.info2("guest call %s", guest_return.c_str());
        JsonObjectPtr rtn(new JsonObject("{}"));
        return rtn;
    }

    JSON_METHOD(list_databases) {
        std::stringstream database_xml;
        MySQLDatabaseListPtr databases = guest->list_databases();
        database_xml << "[";
        BOOST_FOREACH(MySQLDatabasePtr & database, *databases) {
            database_xml << "{'name':'" << database->get_name()
                         << "', 'collation':'" << database->get_collation()
                         << "', 'charset':'" << database->get_charset()
                     << "'},";
        }
        database_xml << "]";
        log.info2("guest call %s", database_xml.str().c_str());
        JsonObjectPtr rtn(new JsonObject("{}"));
        return rtn;
    }

    JSON_METHOD(delete_database) {
        string guest_return = guest->delete_database("database_name");
        log.info2("guest call %s", guest_return.c_str());
        JsonObjectPtr rtn(new JsonObject("{}"));
        return rtn;
    }

    JSON_METHOD(enable_root) {
        string guest_return = guest->enable_root();
        log.info2("roots new password %s", guest_return.c_str());
        JsonObjectPtr rtn(new JsonObject("{}"));
        return rtn;
    }

    JSON_METHOD(disable_root) {
        string guest_return = guest->disable_root();
        log.info2( "guest call %s", guest_return.c_str());
        JsonObjectPtr rtn(new JsonObject("{}"));
        return rtn;
    }

    JSON_METHOD(is_root_enabled) {
        bool enabled = guest->is_root_enabled();
        log.info2( "guest call %i", enabled);
        JsonObjectPtr rtn(new JsonObject("{}"));
        return rtn;
    }

    struct MethodEntry {
        const char * const name;
        const MySqlMessageHandler::MethodPtr ptr;
    };

}

MySqlMessageHandler::MySqlMessageHandler(MySqlGuestPtr guest)
:   guest(guest),
    methods()
{
    const MethodEntry static_method_entries [] = {
        REGISTER(create_database),
        REGISTER(create_user),
        REGISTER(delete_database),
        REGISTER(disable_root),
        REGISTER(delete_user),
        REGISTER(enable_root),
        REGISTER(is_root_enabled),
        REGISTER(list_databases),
        REGISTER(list_users),
        {0, 0}
    };
    const MethodEntry * itr = static_method_entries;
    while(itr->name != 0) {
        log.info2( "Registering method %s", itr->name);
        methods[itr->name] = itr->ptr;
        itr ++;
    }
}

JsonObjectPtr MySqlMessageHandler::handle_message(JsonObjectPtr arguments) {
    string method_name;
    arguments->get_string("method", method_name);

    MethodMap::iterator method_itr = methods.find(method_name);
    if (method_itr != methods.end()) {
        MethodPtr & method = method_itr->second;  // value
        log.info2( "Executing method %s", method_name.c_str());
        return (*(method))(this->guest, arguments);
    } else {
        log.info2( "Couldnt find a method for %s", method_name.c_str());
        return JsonObjectPtr();
    }
}

} } } // end namespace
