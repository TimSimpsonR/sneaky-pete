#include "pch.hpp"
#include "message_handler.h"

#include "nova/guest/apt.h"
#include "nova/backup/BackupRestore.h"
#include "nova/guest/guest.h"
#include "nova/guest/GuestException.h"
#include "nova/process.h"
#include "nova/Log.h"
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <exception>
#include <boost/foreach.hpp>
#include <boost/format.hpp>
#include <boost/assign/list_of.hpp>
#include <boost/assign/std/list.hpp>
#include <boost/optional.hpp>
#include <boost/tuple/tuple.hpp>
#include <boost/variant.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include "client.h"
#include "constants.h"

using namespace boost::assign;

using nova::guest::apt::AptGuest;
using nova::backup::BackupRestoreInfo;
using nova::guest::GuestException;
using nova::Log;
using nova::JsonData;
using nova::JsonDataPtr;
using nova::JsonObject;
using nova::JsonObjectPtr;
using nova::json_string;
using boost::optional;
using namespace std;
using boost::tie;
using nova::redis::REDIS_AGENT_NAME;
using nova::redis::DEFAULT_REDIS_CONFIG;
using nova::guest::common::PrepareHandlerPtr;
using nova::redis::REQUIRE_PASS;
using nova::redis::RedisApp;
using nova::redis::RedisAppPtr;
using nova::redis::RedisAppStatusPtr;

namespace nova { namespace guest {


namespace {

std::string get_uuid()
{
    std::stringstream ss;
    boost::uuids::uuid uuid = boost::uuids::random_generator()();
    ss << uuid;
    return ss.str();
}

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


}//end anon namespace.


RedisMessageHandler::RedisMessageHandler(
    RedisAppPtr app,
    PrepareHandlerPtr prepare_handler,
    RedisAppStatusPtr app_status)
:   app(app),
    app_status(app_status),
    prepare_handler(prepare_handler)
{
}

RedisMessageHandler::~RedisMessageHandler()
{
}

JsonDataPtr RedisMessageHandler::handle_message(const GuestInput & input) {
    if (input.method_name == "prepare")
    {
        prepare_handler->prepare(input);
        return JsonData::from_null();
    }
    else if (input.method_name == "restart")
    {
        app->restart();
        return JsonData::from_null();
    }
    else if (input.method_name == "change_passwords")
    {
        const auto password = input.args->get_array("users")
                                        ->get_object(0)
                                        ->get_string("password");
        app->change_password(password);
        return JsonData::from_null();
    } else if (input.method_name == "reset_configuration") {
        NOVA_LOG_INFO("Resetting config file...");
        auto config = input.args->get_object("configuration");
        auto config_contents = config->get_string("config_contents");
        app->reset_configuration(config_contents);
        return JsonData::from_null();
    } else if (input.method_name == "start_db_with_conf_changes")
    {
        const auto config_contents = input.args->get_string("config_contents");
        app->start_with_conf_changes(config_contents);
        return JsonData::from_null();
    }
    else if (input.method_name == "stop_db")
    {
        app->stop();
        return JsonData::from_null();
    }
    else
    {
        return JsonDataPtr();
    }
}


}}//end namespace nova::guest
