#include "nova/guest/apt.h"
#include <boost/format.hpp>
#include "nova/guest/guest.h"
#include "nova/Log.h"
#include <boost/foreach.hpp>
#include "nova/db/mysql.h"
#include "nova/guest/mysql/MySqlMessageHandler.h"
#include "nova/guest/mysql/MySqlAppStatus.h"
#include "nova/guest/mysql/MySqlApp.h"
#include <sstream>


using nova::guest::apt::AptGuest;
using boost::format;
using nova::Log;
using nova::JsonData;
using nova::JsonDataPtr;
using nova::JsonObject;
using nova::JsonObjectPtr;
using namespace nova::db::mysql;
using namespace std;



namespace nova { namespace guest { namespace mysql {

namespace {

}


/**---------------------------------------------------------------------------
 *- MySqlMessageHandler
 *---------------------------------------------------------------------------*/

#define STR_VALUE(arg)  #arg
#define METHOD_NAME(name) STR_VALUE(name)
#define REGISTER(name) { METHOD_NAME(name), & name }
#define JSON_METHOD(name) JsonDataPtr \
    name(const MySqlMessageHandler * guest, JsonObjectPtr args)


namespace {

    MySqlDatabasePtr db_from_obj(JsonObjectPtr obj) {
        MySqlDatabasePtr db(new MySqlDatabase());
        string char_set = obj->get_string_or_default(
            "_character_set",
            MySqlDatabase::default_character_set());
        db->set_character_set(char_set);
        string collate = obj->get_string_or_default(
            "_collate", MySqlDatabase::default_collation());
        db->set_collation(collate);
        db->set_name(obj->get_string("_name"));
        return db;
    }

    void db_list_from_array(MySqlDatabaseListPtr db_list, JsonArrayPtr array) {
        for (int i = 0; i < array->get_length(); i ++) {
            JsonObjectPtr db_obj = array->get_object(i);
            NOVA_LOG_INFO2("database json info:%s", db_obj->to_string());
            db_list->push_back(db_from_obj(db_obj));
        };
    }

    MySqlUserPtr user_from_obj(JsonObjectPtr obj) {
        MySqlUserPtr user(new MySqlUser());
        user->set_name(obj->get_string("_name"));
        user->set_password(obj->get_optional_string("_password"));
        JsonArrayPtr db_array = obj->get_array("_databases");
        db_list_from_array(user->get_databases(), db_array);
        return user;
    }

    void user_to_stream(stringstream & out, MySqlUserPtr user) {
        out << "{\"_name\":" << JsonData::json_string(user->get_name().c_str())
            << ", \"_password\":";
        if (user->get_password()) {
            out << JsonData::json_string(user->get_password().get().c_str());
        } else {
            out << "null";
        }
        out << " }";
    }

    JsonDataPtr _create_database(MySqlAdminPtr sql, JsonObjectPtr args) {
        NOVA_LOG_INFO2("guest create_database"); //", guest->create_database().c_str());
        MySqlDatabaseListPtr databases(new MySqlDatabaseList());
        JsonArrayPtr array = args->get_array("databases");
        db_list_from_array(databases, array);
        sql->create_database(databases);
        return JsonData::from_null();
    }

    JSON_METHOD(create_database) {
        MySqlAdminPtr sql = guest->sql_admin();
        return _create_database(sql, args);
    }

    JSON_METHOD(create_user) {
        MySqlAdminPtr sql = guest->sql_admin();
        MySqlUserListPtr users(new MySqlUserList());
        JsonArrayPtr array = args->get_array("users");
        for (int i = 0; i < array->get_length(); i ++) {
            users->push_back(user_from_obj(array->get_object(i)));
        };
        sql->create_users(users);
        return JsonData::from_null();
    }

    JSON_METHOD(list_users) {
        MySqlAdminPtr sql = guest->sql_admin();
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
        MySqlAdminPtr sql = guest->sql_admin();
        MySqlUserPtr user = user_from_obj(args->get_object("user"));
        sql->delete_user(user->get_name());
        return JsonData::from_null();
    }

    JSON_METHOD(list_databases) {
        MySqlAdminPtr sql = guest->sql_admin();
        std::stringstream database_xml;
        MySqlDatabaseListPtr databases = sql->list_databases();
        database_xml << "[";
        bool once = false;
        BOOST_FOREACH(MySqlDatabasePtr & database, *databases) {
            if (once) {
                database_xml << ", ";
            }
            once = true;
            database_xml << "{ \"_name\":"
                         << JsonData::json_string(database->get_name())
                         << ", \"_collate\":"
                         << JsonData::json_string(database->get_collation())
                         << ", \"_character_set\":"
                         << JsonData::json_string(database->get_character_set())
                         << " }";
        }
        database_xml << "]";
        JsonDataPtr rtn(new JsonArray(database_xml.str().c_str()));
        return rtn;
    }

    JSON_METHOD(delete_database) {
        MySqlAdminPtr sql = guest->sql_admin();
        MySqlDatabasePtr db = db_from_obj(args->get_object("database"));
        sql->delete_database(db->get_name());
        return JsonData::from_null();
    }

    JSON_METHOD(enable_root) {
        MySqlAdminPtr sql = guest->sql_admin();
        MySqlUserPtr user = sql->enable_root();
        stringstream out;
        user_to_stream(out, user);
        JsonDataPtr rtn(new JsonObject(out.str().c_str()));
        return rtn;
    }

    JSON_METHOD(is_root_enabled) {
        MySqlAdminPtr sql = guest->sql_admin();
        bool enabled = sql->is_root_enabled();
        return JsonData::from_boolean(enabled);
    }

    struct MethodEntry {
        const char * const name;
        const MySqlMessageHandler::MethodPtr ptr;
    };

}

MySqlMessageHandler::MySqlMessageHandler()
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
        {0, 0}
    };
    const MethodEntry * itr = static_method_entries;
    while(itr->name != 0) {
        NOVA_LOG_INFO2( "Registering method %s", itr->name);
        methods[itr->name] = itr->ptr;
        itr ++;
    }
}

JsonDataPtr MySqlMessageHandler::handle_message(const GuestInput & input) {
    MethodMap::iterator method_itr = methods.find(input.method_name);
    if (method_itr != methods.end()) {
        // Make sure our connection is fresh.
        MethodPtr & method = method_itr->second;  // value
        NOVA_LOG_INFO2( "Executing method %s", input.method_name.c_str());
        JsonDataPtr result = (*(method))(this, input.args);
        return result;
    } else {
        return JsonDataPtr();
    }
}


MySqlAdminPtr MySqlMessageHandler::sql_admin() {
    // Creates a connection from local host with values coming from my.cnf.
    MySqlConnectionPtr connection(new MySqlConnection("localhost"));
    MySqlAdminPtr ptr(new MySqlAdmin(connection));
    return ptr;
}



/**---------------------------------------------------------------------------
 *- MySqlAppMessageHandler
 *---------------------------------------------------------------------------*/

MySqlAppMessageHandler::MySqlAppMessageHandler(
    nova::guest::apt::AptGuest & apt,
    MySqlAppStatusPtr status,
    int state_change_wait_time)
: apt(apt),
  status(status),
  state_change_wait_time(state_change_wait_time)
{
}

MySqlAppMessageHandler::~MySqlAppMessageHandler() {

}

JsonDataPtr MySqlAppMessageHandler::handle_message(const GuestInput & input) {
    if (input.method_name == "prepare") {
        NOVA_LOG_INFO("Calling prepare...");
        MySqlAppPtr app = this->create_mysql_app();
        int memory_mb = input.args->get_int("memory_mb");;
        app->install_and_secure(this->apt, memory_mb);
        // The argument signature is the same as create_database so just
        // forward the method.
        NOVA_LOG_INFO("Creating initial databases following successful prepare");
        return _create_database(MySqlMessageHandler::sql_admin(), input.args);
    } else if (input.method_name == "restart") {
        NOVA_LOG_INFO("Calling restart...");
        MySqlAppPtr app = this->create_mysql_app();
        app->restart();
        return JsonData::from_null();
    } else if (input.method_name == "start_mysql_with_conf_changes") {
        NOVA_LOG_INFO("Calling start with conf changes...");
        MySqlAppPtr app = this->create_mysql_app();
        int memory_mb = input.args->get_int("updated_memory_size");
        app->start_mysql_with_conf_changes(this->apt,
                                           memory_mb);
        return JsonData::from_null();
    } else if (input.method_name == "stop_mysql") {
        NOVA_LOG_INFO("Calling stop...");
        MySqlAppPtr app = this->create_mysql_app();
        app->stop_mysql();
        return JsonData::from_null();
    } else {
        return JsonDataPtr();
    }
}

MySqlAppPtr MySqlAppMessageHandler::create_mysql_app()
{
    MySqlAppPtr ptr(new MySqlApp(this->status, this->state_change_wait_time));
    return ptr;
}

} } } // end namespace
