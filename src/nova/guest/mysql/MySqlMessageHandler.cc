#include "nova/guest/apt.h"
#include "nova/guest/guest.h"
#include "nova/Log.h"
#include <boost/foreach.hpp>
#include "nova/guest/mysql.h"
#include "nova/guest/mysql/MySqlPreparer.h"
#include <sstream>


using nova::guest::apt::AptGuest;
using nova::Log;
using nova::JsonData;
using nova::JsonDataPtr;
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
#define JSON_METHOD(name) JsonDataPtr \
    name(AptGuest * apt, const MySqlGuestPtr & sql, JsonObjectPtr args)


namespace {

    MySqlDatabasePtr db_from_obj(JsonObjectPtr obj) {
        MySqlDatabasePtr db(new MySqlDatabase());
        db->set_character_set(obj->get_string("character_set"));
        db->set_collation(obj->get_string("collation"));
        db->set_name(obj->get_string("name"));
        return db;
    }

    MySqlUserPtr user_from_obj(JsonObjectPtr obj) {
        MySqlUserPtr user(new MySqlUser());
        user->set_name(obj->get_string("username"));
        user->set_password(obj->get_string("password"));
        return user;
    }

    void user_to_stream(stringstream & out, MySqlUserPtr user) {
        out << "{'name':'" << user->get_name() << "'}";
    }

    JSON_METHOD(create_database) {
        log.info2("guest create_database"); //", guest->create_database().c_str());
        MySqlDatabaseListPtr databases(new MySqlDatabaseList());
        JsonArrayPtr array = args->get_array("databases");
        for (int i = 0; i < array->get_length(); i ++) {
            databases->push_back(db_from_obj(array->get_object(i)));
        };
        sql->create_database(databases);
        return JsonData::from_null();
    }

    JSON_METHOD(create_user) {
        MySqlUserListPtr users(new MySqlUserList());
        JsonArrayPtr array = args->get_array("users");
        for (int i = 0; i < array->get_length(); i ++) {
            users->push_back(user_from_obj(array->get_object(i)));
        };
        sql->create_users(users);
        return JsonData::from_null();
    }

    JSON_METHOD(list_users) {
        std::stringstream user_xml;
        MySqlUserListPtr users = sql->list_users();
        user_xml << "[";
        bool once = false;
        BOOST_FOREACH(MySqlUserPtr & user, *users) {
            if (once) {
                user_xml << ", ";
            }
            user_to_stream(user_xml, user);
            once = true;
        }
        user_xml << "]";
        JsonDataPtr rtn(new JsonArray(user_xml.str().c_str()));
        return rtn;
    }

    JSON_METHOD(delete_user) {
        MySqlUserPtr user = user_from_obj(args->get_object("user"));
        sql->delete_user(user->get_name());
        return JsonData::from_null();
    }

    JSON_METHOD(list_databases) {
        std::stringstream database_xml;
        MySqlDatabaseListPtr databases = sql->list_databases();
        database_xml << "[";
        bool once = false;
        BOOST_FOREACH(MySqlDatabasePtr & database, *databases) {
            if (once) {
                database_xml << ", ";
            }
            once = true;
            database_xml << "{'name':'" << database->get_name()
                         << "', 'collation':'" << database->get_collation()
                         << "', 'charset':'" << database->get_character_set()
                         << "'}";
        }
        database_xml << "]";
        JsonDataPtr rtn(new JsonArray(database_xml.str().c_str()));
        return rtn;
    }

    JSON_METHOD(delete_database) {
        MySqlDatabasePtr db = db_from_obj(args->get_object("database"));
        sql->delete_database(db->get_name());
        return JsonData::from_null();
    }

    JSON_METHOD(enable_root) {
        MySqlUserPtr user = sql->enable_root();
        stringstream out;
        user_to_stream(out, user);
        JsonDataPtr rtn(new JsonObject(out.str().c_str()));
        return rtn;
    }

    JSON_METHOD(is_root_enabled) {
        bool enabled = sql->is_root_enabled();
        return JsonData::from_boolean(enabled);
    }

    JSON_METHOD(prepare) {
        log.info("Called prepare.");
        MySqlPreparer preparer(apt);
        preparer.prepare();
        // The argument signature is the same as create_database so just
        // forward the method.
        log.info("Creating initial databases following successful prepare");
        return create_database(apt, sql, args);
    }

    struct MethodEntry {
        const char * const name;
        const MySqlMessageHandler::MethodPtr ptr;
    };

}

MySqlMessageHandler::MySqlMessageHandler(MySqlGuestPtr sql_guest,
                                         nova::guest::apt::AptGuest * apt_guest)
:   apt(apt_guest),
    methods(),
    sql(sql_guest)
{
    const MethodEntry static_method_entries [] = {
        REGISTER(create_database),
        REGISTER(create_user),
        REGISTER(delete_database),
        REGISTER(delete_user),
        REGISTER(enable_root),
        REGISTER(is_root_enabled),
        REGISTER(list_databases),
        REGISTER(list_users),
        REGISTER(prepare),
        {0, 0}
    };
    const MethodEntry * itr = static_method_entries;
    while(itr->name != 0) {
        log.info2( "Registering method %s", itr->name);
        methods[itr->name] = itr->ptr;
        itr ++;
    }
}

JsonDataPtr MySqlMessageHandler::handle_message(JsonObjectPtr arguments) {
    string method_name;
    arguments->get_string("method", method_name);

    MethodMap::iterator method_itr = methods.find(method_name);
    if (method_itr != methods.end()) {
        // Make sure our connection is fresh.
        if (method_name != "prepare") {
            this->sql->get_connection()->close();
            this->sql->get_connection()->init();
        }
        MethodPtr & method = method_itr->second;  // value
        log.info2( "Executing method %s", method_name.c_str());
        JsonDataPtr result = (*(method))(this->apt, this->sql, arguments);
        this->sql->get_connection()->close();
        return result;
    } else {
        return JsonDataPtr();
    }
}

} } } // end namespace
