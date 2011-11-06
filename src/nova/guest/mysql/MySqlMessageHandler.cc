#include "nova/guest/apt.h"
#include <boost/format.hpp>
#include "nova/guest/guest.h"
#include "nova/Log.h"
#include <boost/foreach.hpp>
#include "nova/guest/mysql.h"
#include "nova/guest/mysql/MySqlMessageHandler.h"
#include "nova/guest/mysql/MySqlNovaUpdater.h"
#include "nova/guest/mysql/MySqlPreparer.h"
#include <sstream>


using nova::guest::apt::AptGuest;
using boost::format;
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
    name(const MySqlMessageHandler * guest, JsonObjectPtr args)


namespace {

    MySqlDatabasePtr db_from_obj(JsonObjectPtr obj) {
        MySqlDatabasePtr db(new MySqlDatabase());
        db->set_character_set(obj->get_string("_character_set"));
        db->set_collation(obj->get_string("_collation"));
        db->set_name(obj->get_string("_name"));
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
        MySqlGuestPtr sql = guest->sql_admin();
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
        MySqlGuestPtr sql = guest->sql_admin();
        MySqlUserListPtr users(new MySqlUserList());
        JsonArrayPtr array = args->get_array("users");
        for (int i = 0; i < array->get_length(); i ++) {
            users->push_back(user_from_obj(array->get_object(i)));
        };
        sql->create_users(users);
        return JsonData::from_null();
    }

    JSON_METHOD(list_users) {
        MySqlGuestPtr sql = guest->sql_admin();
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
        MySqlGuestPtr sql = guest->sql_admin();
        MySqlUserPtr user = user_from_obj(args->get_object("user"));
        sql->delete_user(user->get_name());
        return JsonData::from_null();
    }

    JSON_METHOD(list_databases) {
        MySqlGuestPtr sql = guest->sql_admin();
        std::stringstream database_xml;
        MySqlDatabaseListPtr databases = sql->list_databases();
        database_xml << "[";
        bool once = false;
        BOOST_FOREACH(MySqlDatabasePtr & database, *databases) {
            if (once) {
                database_xml << ", ";
            }
            once = true;
            database_xml << "{'_name':'" << database->get_name()
                         << "', '_collation':'" << database->get_collation()
                         << "', '_character_set':'" << database->get_character_set()
                         << "'}";
        }
        database_xml << "]";
        JsonDataPtr rtn(new JsonArray(database_xml.str().c_str()));
        return rtn;
    }

    JSON_METHOD(delete_database) {
        MySqlGuestPtr sql = guest->sql_admin();
        MySqlDatabasePtr db = db_from_obj(args->get_object("database"));
        sql->delete_database(db->get_name());
        return JsonData::from_null();
    }

    JSON_METHOD(enable_root) {
        MySqlGuestPtr sql = guest->sql_admin();
        MySqlUserPtr user = sql->enable_root();
        stringstream out;
        user_to_stream(out, user);
        JsonDataPtr rtn(new JsonObject(out.str().c_str()));
        return rtn;
    }

    JSON_METHOD(is_root_enabled) {
        MySqlGuestPtr sql = guest->sql_admin();
        bool enabled = sql->is_root_enabled();
        return JsonData::from_boolean(enabled);
    }

    JSON_METHOD(periodic_tasks) {
        MySqlNovaUpdaterPtr updater = guest->sql_updater();
        MySqlNovaUpdater::Status status = updater->get_local_db_status();
        updater->update_status(status);
        return JsonData::from_null();
    }

    JSON_METHOD(prepare) {
        log.info("Prepare was called.");
        log.info("Updating status to BUILDING...");
        {
            MySqlNovaUpdaterPtr updater = guest->sql_updater();
            updater->update_status(MySqlNovaUpdater::BUILDING);
        }
        log.info("Calling prepare...");
        {
            MySqlPreparerPtr preparer = guest->sql_preparer();
            preparer->prepare();
        }
        // The argument signature is the same as create_database so just
        // forward the method.
        log.info("Creating initial databases following successful prepare");
        log.info2("Here's the args: %s", args->to_string());
        return create_database(guest, args);
    }

    struct MethodEntry {
        const char * const name;
        const MySqlMessageHandler::MethodPtr ptr;
    };

}

MySqlMessageHandler::MySqlMessageHandler(MySqlMessageHandlerConfig config)
:   config(config)
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
        REGISTER(periodic_tasks),
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

JsonDataPtr MySqlMessageHandler::handle_message(const std::string & method_name,
                                                JsonObjectPtr args) {
    MethodMap::iterator method_itr = methods.find(method_name);
    if (method_itr != methods.end()) {
        // Make sure our connection is fresh.
        MethodPtr & method = method_itr->second;  // value
        log.info2( "Executing method %s", method_name.c_str());
        JsonDataPtr result = (*(method))(this, args);
        return result;
    } else {
        return JsonDataPtr();
    }
}

MySqlGuestPtr MySqlMessageHandler::sql_admin() const {
    // Creates a connection from local host with values coming from my.cnf.
    MySqlConnectionPtr connection(new MySqlConnection("localhost"));
    MySqlGuestPtr ptr(new MySqlGuest(connection));
    return ptr;
}

MySqlPreparerPtr MySqlMessageHandler::sql_preparer() const {
    MySqlPreparerPtr ptr(new MySqlPreparer(config.apt));
    return ptr;
}

MySqlNovaUpdaterPtr MySqlMessageHandler::sql_updater() const {
    // Ensure a fresh connection.
    config.nova_db->close();
    config.nova_db->init();
    // Begin using the nova database.
    string using_stmt = str(format("use %s") % config.nova_db_name);
    config.nova_db->query(using_stmt.c_str());
    MySqlNovaUpdaterPtr ptr(new MySqlNovaUpdater(
        config.nova_db, config.guest_ethernet_device.c_str()));
    return ptr;
}

} } } // end namespace
