#include "pch.hpp"
#include "nova/guest/apt.h"
#include "nova/guest/backup/BackupRestore.h"
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
#include <sstream>
#include <boost/tuple/tuple.hpp>
#include <boost/variant.hpp>
#include "nova/VolumeManager.h"

using namespace boost::assign;

using nova::guest::apt::AptGuest;
using nova::guest::backup::BackupRestoreInfo;
using boost::format;
using nova::guest::GuestException;
using nova::Log;
using nova::JsonData;
using nova::JsonDataPtr;
using nova::JsonObject;
using nova::JsonObjectPtr;
using nova::guest::mysql::MySqlGuestException;
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
                NOVA_LOG_INFO("dbs_to_stream: %s", database->get_name().c_str());
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
            NOVA_LOG_INFO("returning: %s", db_stream.str().c_str());
            out << db_stream.str().c_str();
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
        NOVA_LOG_INFO("list access: %s", json.str().c_str());
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
    MySqlAppPtr mysqlApp,
    nova::guest::apt::AptGuestPtr apt,
    nova::guest::monitoring::Monitoring & monitoring,
    VolumeManagerPtr volumeManager)
:   apt(apt),
    monitoring(monitoring),
    mysqlApp(mysqlApp),
    volumeManager(volumeManager)
{
}

MySqlAppMessageHandler::~MySqlAppMessageHandler() {

}

JsonDataPtr MySqlAppMessageHandler::handle_message(const GuestInput & input) {
    if (input.method_name == "prepare") {
        NOVA_LOG_INFO("Calling prepare...");
        MySqlAppPtr app = this->create_mysql_app();
        const auto config_contents = input.args->get_string("config_contents");
        const auto overrides = input.args->get_optional_string("overrides");
        // Mount volume
        if (volumeManager) {
            const auto device_path = input.args->get_optional_string("device_path");
            const auto mount_point = input.args->get_optional_string("mount_point");
            if (device_path && device_path.get().length() > 0) {
                NOVA_LOG_INFO("Mounting volume for prepare call...");
                bool write_to_fstab = true;
                VolumeManagerPtr volume_manager = this->create_volume_manager();
                VolumeDevice vol_device = volume_manager->create_volume_device(device_path.get());
                vol_device.format();
                vol_device.mount(mount_point.get(), write_to_fstab);
                NOVA_LOG_INFO("Mounted the volume.");
            }
        }
        // Restore the database?
        optional<BackupRestoreInfo> restore;
        const auto backup_url = input.args->get_optional_string("backup_url");
        if (backup_url && backup_url.get().length() > 0) {
            NOVA_LOG_INFO("Calling Restore...")
            if (!input.token) {
                NOVA_LOG_ERROR("No token given! Cannot do this restore!");
                throw GuestException(GuestException::MALFORMED_INPUT);
            }
            const auto token = input.token;
            const auto backup_checksum = input.args->get_optional_string("backup_checksum");
            restore = optional<BackupRestoreInfo>(
                BackupRestoreInfo(token.get(), backup_url.get(), backup_checksum.get()));
        }
        app->prepare(*this->apt, config_contents, overrides, restore);

        // The argument signature is the same as create_database so just
        // forward the method.
        NOVA_LOG_INFO("Creating initial databases and users following successful prepare");
        _create_database(MySqlMessageHandler::sql_admin(), input.args);
        _create_user(MySqlMessageHandler::sql_admin(), input.args);

        // installation of monitoring
        const auto monitoring_info =
            input.args->get_optional_object("monitoring_info");
        if (monitoring_info) {
            NOVA_LOG_INFO("Installing Monitoring Agent following successful prepare");
            const auto token = monitoring_info->get_string("token");
            const auto endpoints = monitoring_info->get_string("endpoints");
            monitoring.install_and_configure_monitoring_agent(
                *this->apt, token, endpoints);
        } else {
            NOVA_LOG_INFO("Skipping Monitoring Agent as no endpoints were supplied.");
        }
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
        app->start_db_with_conf_changes(*this->apt, config_contents);
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
        app->reset_configuration(*this->apt, config_contents);
        return JsonData::from_null();
    } else if (input.method_name == "stop_db") {
        NOVA_LOG_INFO("Calling stop...");
        bool do_not_start_on_reboot =
            input.args->get_optional_bool("do_not_start_on_reboot")
            .get_value_or(false);
        MySqlAppPtr app = this->create_mysql_app();
        app->stop_db(do_not_start_on_reboot);
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
            const auto mount_point = input.args->get_optional_string("mount_point");
            VolumeManagerPtr volume_manager = this->create_volume_manager();
            VolumeDevice vol_device = volume_manager->create_volume_device(device_path.get());

            vol_device.mount(mount_point.get(), write_to_fstab);
        }

        return JsonData::from_null();
    } else if (input.method_name == "unmount_volume") {
        NOVA_LOG_INFO("Calling unmount_volume...");
        if (volumeManager) {
            const auto device_path = input.args->get_optional_string("device_path");
            const auto mount_point = input.args->get_optional_string("mount_point");
            VolumeManagerPtr volume_manager = this->create_volume_manager();
            VolumeDevice vol_device = volume_manager->create_volume_device(device_path.get());

            vol_device.unmount(mount_point.get());
        }

        return JsonData::from_null();
    } else if (input.method_name == "resize_fs") {
        NOVA_LOG_INFO("Calling resize_fs...");
        if (volumeManager) {
            const auto device_path = input.args->get_optional_string("device_path");
            const auto mount_point = input.args->get_optional_string("mount_point");
            VolumeManagerPtr volume_manager = this->create_volume_manager();
            VolumeDevice vol_device = volume_manager->create_volume_device(device_path.get());

            vol_device.resize_fs(mount_point.get());
        }

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
