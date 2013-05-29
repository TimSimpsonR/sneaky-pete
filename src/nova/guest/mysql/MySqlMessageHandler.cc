#include "nova/guest/apt.h"
#include <boost/format.hpp>
#include "nova/guest/guest.h"
#include "nova/Log.h"
#include <boost/foreach.hpp>
#include "nova/db/mysql.h"
#include "nova/guest/mysql/MySqlMessageHandler.h"
#include "nova/guest/mysql/MySqlAppStatus.h"
#include "nova/guest/mysql/MySqlApp.h"
#include "nova/guest/monitoring/monitoring.h"
#include <boost/optional.hpp>
#include <sstream>
#include <boost/tuple/tuple.hpp>


using nova::guest::apt::AptGuest;
using boost::format;
using nova::Log;
using nova::JsonData;
using nova::JsonDataPtr;
using nova::JsonObject;
using nova::JsonObjectPtr;
using namespace nova::db::mysql;
using boost::optional;
using namespace std;
using boost::tie;
using namespace nova::guest::monitoring;



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
        user->set_host(obj->get_optional_string("_host").get_value_or("%"));
        user->set_password(obj->get_optional_string("_password"));
        JsonArrayPtr db_array = obj->get_array("_databases");
        db_list_from_array(user->get_databases(), db_array);
        return user;
    }

    MySqlUserPtr user_update_from_obj(JsonObjectPtr obj) {
        MySqlUserPtr user(new MySqlUser());
        user->set_name(obj->get_string("name"));
        user->set_host(obj->get_optional_string("host").get_value_or("%"));
        user->set_password(obj->get_optional_string("password"));
        return user;
    }

    void user_to_stream(stringstream & out, MySqlUserPtr user) {
        out << "{\"_name\":" << JsonData::json_string(user->get_name().c_str())
            << ", \"_host\":" << JsonData::json_string(user->get_host().c_str())
            << ", \"_password\":";
        if (user->get_password()) {
            out << JsonData::json_string(user->get_password().get().c_str());
        } else {
            out << "null";
        }
        out << ", \"_databases\":";
        if (user->get_databases()) {
            std::stringstream database_xml;
            database_xml << "[";
            bool once = false;
            BOOST_FOREACH(MySqlDatabasePtr & database, *user->get_databases()) {
                if (once) {
                    database_xml << ", ";
                }
                once = true;
                database_xml << "{ \"_name\":"
                             << JsonData::json_string(database->get_name())
                             << " }";
            }
            database_xml << "]";
            out << database_xml.str().c_str();
        } else {
            out << "null";
        }
        out << " }";
    }

    void dbs_to_stream(stringstream & out, MySqlDatabaseListPtr dbs) {
            std::stringstream db_stream;
            db_stream << "[";
            bool once = false;
            BOOST_FOREACH(MySqlDatabasePtr & database, *dbs) {
                NOVA_LOG_INFO2("dbs_to_stream: %s", database->get_name().c_str());
                if (once) {
                    db_stream << ", ";
                }
                once = true;
                db_stream << "{ \"_name\":"
                          << JsonData::json_string(database->get_name())
                          << ", \"_collate\": \"\""
                          << ", \"_character_set\": \"\""
                          << " }";
            }
            db_stream << "]";
            NOVA_LOG_INFO2("returning: %s", db_stream.str().c_str());
            out << db_stream.str().c_str();
    }

    JsonDataPtr _create_database(MySqlAdminPtr sql, JsonObjectPtr args) {
        NOVA_LOG_INFO2("guest create_database"); //", guest->create_database().c_str());
        MySqlDatabaseListPtr databases(new MySqlDatabaseList());
        JsonArrayPtr array = args->get_array("databases");
        db_list_from_array(databases, array);
        sql->create_database(databases);
        return JsonData::from_null();
    }

    JsonDataPtr _create_user(MySqlAdminPtr sql, JsonObjectPtr args) {
        NOVA_LOG_INFO2("guest create_user");
        MySqlUserListPtr users(new MySqlUserList());
        JsonArrayPtr array = args->get_array("users");
        for (int i = 0; i < array->get_length(); i ++) {
            users->push_back(user_from_obj(array->get_object(i)));
        };
        sql->create_users(users);
        return JsonData::from_null();
    }

    JSON_METHOD(create_database) {
        MySqlAdminPtr sql = guest->sql_admin();
        return _create_database(sql, args);
    }

    JSON_METHOD(create_user) {
        MySqlAdminPtr sql = guest->sql_admin();
        return _create_user(sql, args);
    }

    JSON_METHOD(list_users) {
        unsigned int limit = args->get_positive_int("limit");
        optional<string> marker = args->get_optional_string("marker");
        bool include_marker = args->get_optional_bool("include_marker")
            .get_value_or(false);

        MySqlAdminPtr sql = guest->sql_admin();
        MySqlUserListPtr users;
        optional<string> next_marker;
        boost::tie(users, next_marker) = sql->list_users(limit, marker,
                                                         include_marker);

        std::stringstream json;
        json << "[";
            // Element 0 = users array.
            json << "[";
                bool once = false;
                BOOST_FOREACH(MySqlUserPtr & user, *users) {
                    if (once) {
                        json << ", ";
                    }
                    user_to_stream(json, user);
                    once = true;
                }
            json << "], ";
            // Element 1 = next_marker
            if (next_marker) {
                json << JsonData::json_string(next_marker.get());
            } else {
                json << "null";
            }
        json << "]";
        JsonDataPtr rtn(new JsonArray(json.str().c_str()));
        return rtn;
    }

    JSON_METHOD(delete_user) {
        MySqlAdminPtr sql = guest->sql_admin();
        MySqlUserPtr user = user_from_obj(args->get_object("user"));
        sql->delete_user(user->get_name(), user->get_host());
        return JsonData::from_null();
    }

    JSON_METHOD(list_databases) {
        unsigned int limit = args->get_positive_int("limit");
        optional<string> marker = args->get_optional_string("marker");
        bool include_marker = args->get_optional_bool("include_marker")
            .get_value_or(false);

        MySqlAdminPtr sql = guest->sql_admin();
        MySqlDatabaseListPtr databases;
        optional<string> next_marker;
        boost::tie(databases, next_marker) = sql->list_databases(limit,
            marker, include_marker);

        std::stringstream json;
        json << "[";
            // Element 0 = database list.
            json << "[";
                bool once = false;
                BOOST_FOREACH(MySqlDatabasePtr & database, *databases) {
                    if (once) {
                        json << ", ";
                    }
                    once = true;
                    json << "{ \"_name\":"
                         << JsonData::json_string(database->get_name())
                         << ", \"_collate\":"
                         << JsonData::json_string(database->get_collation())
                         << ", \"_character_set\":"
                         << JsonData::json_string(database->get_character_set())
                         << " }";
                }
            json << "], ";
            // Element 1 = next marker
            if (next_marker) {
                json << JsonData::json_string(next_marker.get());
            } else {
                json << "null";
            }
        json << "]";
        JsonDataPtr rtn(new JsonArray(json.str().c_str()));
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

    JSON_METHOD(change_passwords) {
        MySqlAdminPtr sql = guest->sql_admin();
        MySqlUserListPtr users(new MySqlUserList());
        JsonArrayPtr array = args->get_array("users");
        for (int i = 0; i < array->get_length(); i ++) {
            users->push_back(user_update_from_obj(array->get_object(i)));
        };
        sql->change_passwords(users);
        return JsonData::from_null();
    }

    JSON_METHOD(get_user){
        MySqlAdminPtr sql = guest->sql_admin();
        string username = args->get_string("username");
        string hostname = args->get_optional_string("hostname").get_value_or("%");
        MySqlUserPtr user = sql->find_user(username, hostname);
        std::stringstream json;
        user_to_stream(json, user);
        JsonDataPtr rtn(new JsonObject(json.str().c_str()));
        return rtn;
    }

    JSON_METHOD(list_access){
        MySqlAdminPtr sql = guest->sql_admin();
        string username = args->get_string("username");
        string hostname = args->get_optional_string("hostname").get_value_or("%");
        MySqlUserPtr user = sql->find_user(username, hostname);

        MySqlDatabaseListPtr dbs = user->get_databases();
        std::stringstream json;
        dbs_to_stream(json, dbs);
        NOVA_LOG_INFO2("list access: %s", json.str().c_str());
        JsonDataPtr rtn(new JsonArray(json.str().c_str()));
        return rtn;

    }

    JSON_METHOD(grant_access){
        MySqlAdminPtr sql = guest->sql_admin();
        string username = args->get_string("username");
        string hostname = args->get_optional_string("hostname").get_value_or("%");
        JsonArrayPtr db_array = args->get_array("databases");
        MySqlDatabaseListPtr dbs(new MySqlDatabaseList());
        for (int i = 0; i < db_array->get_length(); i ++) {
            std::string db_name = db_array->get_string(i);
            MySqlDatabasePtr db(new MySqlDatabase());
            db->set_name(db_name);
            db->set_character_set("");
            db->set_collation("");
            dbs->push_back((db));
        };
        sql->grant_access(username, hostname, dbs);
        return JsonData::from_null();
    }

    JSON_METHOD(revoke_access){
        MySqlAdminPtr sql = guest->sql_admin();
        string username = args->get_string("username");
        string hostname = args->get_optional_string("hostname").get_value_or("%");
        string database = args->get_string("database");
        sql->revoke_access(username, hostname, database);
        return JsonData::from_null();
    }

    struct MethodEntry {
        const char * const name;
        const MySqlMessageHandler::MethodPtr ptr;
    };

}

MySqlMessageHandler::MySqlMessageHandler()
{
    const MethodEntry static_method_entries [] = {
        REGISTER(change_passwords),
        REGISTER(create_database),
        REGISTER(create_user),
        REGISTER(delete_database),
        REGISTER(delete_user),
        REGISTER(enable_root),
        REGISTER(get_user),
        REGISTER(grant_access),
        REGISTER(is_root_enabled),
        REGISTER(list_access),
        REGISTER(list_databases),
        REGISTER(list_users),
        REGISTER(revoke_access),
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
    int state_change_wait_time,
    nova::guest::monitoring::Monitoring & monitoring)
: apt(apt),
  status(status),
  state_change_wait_time(state_change_wait_time),
  monitoring(monitoring)
{
}

MySqlAppMessageHandler::~MySqlAppMessageHandler() {

}

JsonDataPtr MySqlAppMessageHandler::handle_message(const GuestInput & input) {
    if (input.method_name == "prepare") {
        NOVA_LOG_INFO("Calling prepare...");
        MySqlAppPtr app = this->create_mysql_app();
        int memory_mb = input.args->get_int("memory_mb");
        app->install_and_secure(this->apt, memory_mb);
        // The argument signature is the same as create_database so just
        // forward the method.
        NOVA_LOG_INFO("Creating initial databases and users following successful prepare");
        _create_database(MySqlMessageHandler::sql_admin(), input.args);
        _create_user(MySqlMessageHandler::sql_admin(), input.args);

        // installation of monitoring
        NOVA_LOG_INFO("Installing Monitoring Agent following successful prepare");
        const auto  monitoring_token = input.args->get_string("monitoring_token");
        const auto  monitoring_endpoints = input.args->get_string("monitoring_endpoints");
        monitoring.install_and_configure_monitoring_agent(this->apt,
                                                           monitoring_token,
                                                           monitoring_endpoints);
        return JsonData::from_null();
    } else if (input.method_name == "restart") {
        NOVA_LOG_INFO("Calling restart...");
        MySqlAppPtr app = this->create_mysql_app();
        app->restart();
        return JsonData::from_null();
    } else if (input.method_name == "start_db_with_conf_changes") {
        NOVA_LOG_INFO("Calling start with conf changes...");
        MySqlAppPtr app = this->create_mysql_app();
        int memory_mb = input.args->get_int("updated_memory_size");
        app->start_db_with_conf_changes(this->apt,
                                           memory_mb);
        return JsonData::from_null();
    } else if (input.method_name == "change_conf_file") {
        NOVA_LOG_INFO("Changing conf file...");
        MySqlAppPtr app = this->create_mysql_app();
        int memory_mb = input.args->get_int("updated_memory_size");
        app->change_conf_file(this->apt, memory_mb);
        return JsonData::from_null();
    } else if (input.method_name == "stop_db") {
        NOVA_LOG_INFO("Calling stop...");
        bool do_not_start_on_reboot =
            input.args->get_optional_bool("do_not_start_on_reboot")
            .get_value_or(false);
        MySqlAppPtr app = this->create_mysql_app();
        app->stop_db(do_not_start_on_reboot);
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
