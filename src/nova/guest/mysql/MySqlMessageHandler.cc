#include "pch.hpp"
#include "nova/guest/apt.h"
#include "nova/backup/BackupRestore.h"
#include <boost/format.hpp>
#include "nova/guest/guest.h"
#include "nova/guest/GuestException.h"
#include "nova/process.h"
#include "nova/Log.h"
#include <boost/foreach.hpp>
#include <boost/assign/list_of.hpp>
#include <boost/assign/std/list.hpp>
#include "nova/db/mysql.h"
#include "nova/guest/mysql/MySqlMessageHandler.h"
#include "nova/guest/mysql/MySqlAppStatus.h"
#include "nova/guest/mysql/MySqlApp.h"
#include "nova/guest/mysql/MySqlGuestException.h"
#include "nova/guest/monitoring/monitoring.h"
#include <boost/optional.hpp>
#include <boost/tuple/tuple.hpp>
#include <boost/variant.hpp>
#include "nova/VolumeManager.h"

using namespace boost::assign;

using nova::guest::apt::AptGuest;
using nova::backup::BackupRestoreInfo;
using boost::format;
using nova::guest::GuestException;
using boost::lexical_cast;
using nova::Log;
using nova::JsonData;
using nova::JsonDataPtr;
using nova::JsonObject;
using nova::JsonObjectPtr;
using nova::json_string;
using nova::guest::mysql::MySqlGuestException;
using namespace nova::db::mysql;
using boost::optional;
using nova::guest::common::PrepareHandler;
using nova::guest::common::PrepareHandlerPtr;
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
            NOVA_LOG_INFO("database json info:%s", db_obj->to_string());
            db_list->push_back(db_from_obj(db_obj));
        };
    }

    /* Convert JsonData to a ServerVariableValue. */
    ServerVariableValue server_variable_value_from_json(const JsonData & value) {
        struct JsonToServerVariableValueVisitor : public JsonData::Visitor {
            ServerVariableValue result;

            JsonToServerVariableValueVisitor()
            :   result(boost::blank()) {
            }

            void for_boolean(bool value) { result = value; }

            void for_string(const char * value) { result = string(value); }

            void for_double(double value) { result = value; }

            void for_int(int value) { result = value; }

            void for_null() { result = boost::blank(); }

            void for_object(JsonObjectPtr object) {
                NOVA_LOG_ERROR("Can't convert JSON object to MySqlServer variable!");
                throw GuestException(GuestException::MALFORMED_INPUT);
            }

            void for_array(JsonArrayPtr array) {
                NOVA_LOG_ERROR("Can't convert JSON array to MySqlServer variable!");
                throw GuestException(GuestException::MALFORMED_INPUT);
            }
        } visitor;
        value.visit(visitor);
        return visitor.result;
    }

    /* Convert a JsonObject into a MySqlServerAssignments instance. */
    MySqlServerAssignments server_assignments_from_obj(JsonObject & object) {
        MySqlServerAssignments assignments;
        struct Itr : public JsonObject::Iterator {

            MySqlServerAssignments & assignments;

            Itr(MySqlServerAssignments & assignments)
            :   assignments(assignments) {
            }

            void for_each(const char * key, const JsonData & value) {
                assignments[key] = server_variable_value_from_json(value);
            }
        } itr(assignments);
        object.iterate(itr);
        return assignments;
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

    MySqlUserAttrPtr user_updateattrs_from_obj(JsonObjectPtr obj) {
        MySqlUserAttrPtr user(new MySqlUserAttr());
        user->set_name(obj->get_optional_string("name"));
        user->set_host(obj->get_optional_string("host"));
        user->set_password(obj->get_optional_string("password"));
        return user;
    }

    optional<JsonArrayBuilder> simple_database_list_to_json_array(
        MySqlDatabaseListPtr dbs)
    {
        if (dbs) {
            JsonArrayBuilder array;
            BOOST_FOREACH(MySqlDatabasePtr & database, *dbs) {
                array.add(json_obj(
                    "_name", database->get_name()
                ));
            }
            return array;
        } else {
            return boost::none;
        }
    }

    JsonArrayBuilder database_list_to_json_array(MySqlDatabaseListPtr dbs) {
        JsonArrayBuilder array;
        BOOST_FOREACH(MySqlDatabasePtr & database, *dbs) {
            array.add(json_obj(
                "_name", database->get_name(),
                "_collate", database->get_collation(),
                "_character_set", database->get_character_set()
            ));
        }
        return array;
    }

    JsonObjectBuilder user_to_json_obj(MySqlUserPtr user) {
        return json_obj(
            "_name", user->get_name().c_str(),
            "_host", user->get_host().c_str(),
            "_password", user->get_password(),
            "_databases", simple_database_list_to_json_array(user->get_databases())
        );
    }

    optional<JsonArrayBuilder> user_list_to_json_array(MySqlUserListPtr users) {
        if (users) {
            JsonArrayBuilder array;
            BOOST_FOREACH(MySqlUserPtr & user, *users) {
                array.add(user_to_json_obj(user));
            }
            return array;
        } else {
            return boost::none;
        }
    }


    JsonDataPtr _create_database(MySqlAdminPtr sql, JsonObjectPtr args) {
        NOVA_LOG_INFO("guest create_database"); //", guest->create_database().c_str());
        MySqlDatabaseListPtr databases(new MySqlDatabaseList());
        JsonArrayPtr array = args->get_array("databases");
        db_list_from_array(databases, array);
        sql->create_database(databases);
        return JsonData::from_null();
    }

    JsonDataPtr _create_user(MySqlAdminPtr sql, JsonObjectPtr args) {
        NOVA_LOG_INFO("guest create_user");
        MySqlUserListPtr users(new MySqlUserList());
        JsonArrayPtr array = args->get_array("users");
        for (int i = 0; i < array->get_length(); i ++) {
            users->push_back(user_from_obj(array->get_object(i)));
        };
        sql->create_users(users);
        return JsonData::from_null();
    }

    JSON_METHOD(apply_overrides) {
        NOVA_LOG_INFO( "apply_overrides method");
        MySqlAdminPtr sql = guest->sql_admin();
        const JsonObjectPtr obj = args->get_object("overrides");
        MySqlServerAssignments assignments = server_assignments_from_obj(*obj);
        sql->set_globals(assignments);
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

    JSON_METHOD(create_user_with_replication_client) {
        MySqlAdminPtr sql = guest->sql_admin();
        MySqlUserPtr user = user_from_obj(args->get_object("user"));
        NOVA_LOG_INFO("Creating strange and mysterious user.");
        sql->create_user(user);
        NOVA_LOG_INFO("Adding replication client grant.");
        sql->create_user(user, "replication client");
        return JsonData::from_null();
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
        JsonDataPtr rtn(new JsonArray(json_array(
            user_list_to_json_array(users),  // element 0 - the user list
            next_marker                      // element 1 - next marker
        )));
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

        JsonDataPtr rtn(new JsonArray(json_array(
            database_list_to_json_array(databases),
            next_marker
        )));
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
        JsonDataPtr rtn(new JsonObject(user_to_json_obj(user)));
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

    JSON_METHOD(update_attributes) {
        MySqlAdminPtr sql = guest->sql_admin();
        string username = args->get_string("username");
        string hostname = args->get_optional_string("hostname").get_value_or("%");
        MySqlUserAttrPtr user = user_updateattrs_from_obj(args->get_object("user_attrs"));
        sql->update_attributes(username, hostname, user);
        return JsonData::from_null();
    }

    JSON_METHOD(get_user){
        MySqlAdminPtr sql = guest->sql_admin();
        string username = args->get_string("username");
        string hostname = args->get_optional_string("hostname").get_value_or("%");
        MySqlUserPtr user;
        try {
            user = sql->find_user(username, hostname);
        } catch(const MySqlGuestException & mse) {
            if (mse.code == MySqlGuestException::USER_NOT_FOUND) {
                return JsonData::from_null();
            }
            NOVA_LOG_ERROR("An unexpected error occurred when finding the user"
                           " (possibly the user name or host was malformed).");
            throw;
        }
        JsonDataPtr rtn(new JsonObject(user_to_json_obj(user)));
        return rtn;
    }

    JSON_METHOD(list_access){
        MySqlAdminPtr sql = guest->sql_admin();
        string username = args->get_string("username");
        string hostname = args->get_optional_string("hostname").get_value_or("%");
        MySqlUserPtr user = sql->find_user(username, hostname);

        MySqlDatabaseListPtr dbs = user->get_databases();

        JsonDataPtr rtn(new JsonArray(database_list_to_json_array(dbs)));
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


    // Grabs the packages argument from a JSON object.
    vector<string> get_packages_argument(JsonObjectPtr obj) {
        try {
            const auto packages = obj->get_array("packages")->to_string_vector();
            return packages;
        } catch(const JsonException) {
            NOVA_LOG_DEBUG("Interpretting \"packages\" as a single string.");
            vector<string> packages;
            packages.push_back(obj->get_string("packages"));
            return packages;
        }
    }
}

MySqlMessageHandler::MySqlMessageHandler()
{
    const MethodEntry static_method_entries [] = {
        REGISTER(apply_overrides),
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
        REGISTER(update_attributes),
        REGISTER(create_user_with_replication_client),
        {0, 0}
    };
    const MethodEntry * itr = static_method_entries;
    while(itr->name != 0) {
        NOVA_LOG_INFO( "Registering method %s", itr->name);
        methods[itr->name] = itr->ptr;
        itr ++;
    }
}

JsonDataPtr MySqlMessageHandler::handle_message(const GuestInput & input) {
    MethodMap::iterator method_itr = methods.find(input.method_name);
    if (method_itr != methods.end()) {
        // Make sure our connection is fresh.
        MethodPtr & method = method_itr->second;  // value
        NOVA_LOG_INFO( "Executing method %s", input.method_name.c_str());
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
    PrepareHandlerPtr prepare_handler,
    MySqlAppPtr mysqlApp,
    nova::guest::apt::AptGuestPtr apt,
    nova::guest::monitoring::MonitoringManagerPtr monitoring,
    VolumeManagerPtr volumeManager)
:   apt(apt),
    monitoring(monitoring),
    mysqlApp(mysqlApp),
    prepare_handler(prepare_handler),
    volumeManager(volumeManager)
{
}

MySqlAppMessageHandler::~MySqlAppMessageHandler() {

}

JsonDataPtr MySqlAppMessageHandler::handle_message(const GuestInput & input) {
    if (input.method_name == "prepare") {
        prepare_handler->prepare(input);
        // The argument signature is the same as create_database so just
        // forward the method.
        NOVA_LOG_INFO("Creating initial databases and users following successful prepare");
        _create_database(MySqlMessageHandler::sql_admin(), input.args);
        _create_user(MySqlMessageHandler::sql_admin(), input.args);

        return JsonData::from_null();
    } else if (input.method_name == "restart") {
        NOVA_LOG_INFO("Calling restart...");
        MySqlAppPtr app = this->create_mysql_app();
        app->restart();
        return JsonData::from_null();
    } else if (input.method_name == "start_db_with_conf_changes") {
        NOVA_LOG_INFO("Calling start with conf changes...");
        MySqlAppPtr app = this->create_mysql_app();
        string config_contents = input.args->get_string("config_contents");
        app->start_db_with_conf_changes(config_contents);
        return JsonData::from_null();
    } else if (input.method_name == "remove_overrides") {
        NOVA_LOG_INFO("Removing overrides file...");
        MySqlAppPtr app = this->create_mysql_app();
        app->remove_overrides();
        return JsonData::from_null();
    } else if (input.method_name == "reset_configuration") {
        NOVA_LOG_INFO("Resetting config file...");
        MySqlAppPtr app = this->create_mysql_app();
        JsonObjectPtr config = input.args->get_object("configuration");
        string config_contents = config->get_string("config_contents");
        app->reset_configuration(config_contents);
        return JsonData::from_null();
    } else if (input.method_name == "stop_db") {
        NOVA_LOG_INFO("Calling stop...");
        bool do_not_start_on_reboot =
            input.args->get_optional_bool("do_not_start_on_reboot")
            .get_value_or(false);
        MySqlAppPtr app = this->create_mysql_app();
        app->stop(do_not_start_on_reboot);
        return JsonData::from_null();
    } else if (input.method_name == "update_overrides") {
        NOVA_LOG_INFO( "update_overrides method");
        MySqlAppPtr app = this->create_mysql_app();
        const string overrides = input.args->get_string("overrides");
        app->write_config_overrides(overrides);
        return JsonData::from_null();
    } else if (input.method_name == "mount_volume") {
        NOVA_LOG_INFO("Calling mount_volume...");
        if (volumeManager) {
            bool write_to_fstab = false;
            const auto device_path = input.args->get_optional_string("device_path");
            const auto mount_point = volumeManager->get_mount_point();
            VolumeManagerPtr volume_manager = this->create_volume_manager();
            VolumeDevice vol_device = volume_manager->create_volume_device(device_path.get());

            vol_device.mount(mount_point, write_to_fstab);
        }

        return JsonData::from_null();
    } else if (input.method_name == "unmount_volume") {
        NOVA_LOG_INFO("Calling unmount_volume...");
        if (volumeManager) {
            const auto device_path = input.args->get_optional_string("device_path");
            const auto mount_point = volumeManager->get_mount_point();
            VolumeManagerPtr volume_manager = this->create_volume_manager();
            VolumeDevice vol_device = volume_manager->create_volume_device(device_path.get());

            vol_device.unmount(mount_point);
        }

        return JsonData::from_null();
    } else if (input.method_name == "resize_fs") {
        NOVA_LOG_INFO("Calling resize_fs...");
        if (volumeManager) {
            const auto device_path = input.args->get_optional_string("device_path");
            const auto mount_point = volumeManager->get_mount_point();
            VolumeManagerPtr volume_manager = this->create_volume_manager();
            VolumeDevice vol_device = volume_manager->create_volume_device(device_path.get());

            vol_device.resize_fs(mount_point);
        }

        return JsonData::from_null();

    } else if (input.method_name == "enable_ssl") {
        NOVA_LOG_INFO("Calling write_ssl_files.");
        JsonObjectPtr ssl_info = input.args->get_object("ssl");
        string private_key = ssl_info->get_string("private_key");
        string public_key = ssl_info->get_string("public_key");
        string ca_certificate = ssl_info->get_string("ca_certificate");
        MySqlAppPtr app = this->create_mysql_app();
        app->write_ssl_files(ca_certificate, private_key, public_key);

        return JsonData::from_null();
    } else {
        return JsonDataPtr();
    }
}

MySqlAppPtr MySqlAppMessageHandler::create_mysql_app()
{
    return mysqlApp;
}

VolumeManagerPtr MySqlAppMessageHandler::create_volume_manager()
{
    return volumeManager;
}

} } } // end namespace
