#define BOOST_TEST_MODULE nova_db_api_tests
#include <boost/test/unit_test.hpp>

#include "nova/db/api.h"
#include "nova/flags.h"
#include <boost/optional.hpp>
#include "nova/Log.h"
#include "nova/db/mysql.h"
#include "nova/guest/utils.h"

#define CHECK_POINT() BOOST_CHECK_EQUAL(2,2);

using nova::db::Api;
using nova::db::ApiPtr;
using namespace nova::flags;
using nova::guest::utils::IsoDateTime;
using nova::Log;
using namespace nova::db::mysql;
using boost::optional;
using nova::db::NewService;
using nova::db::Service;
using nova::db::ServicePtr;
using std::string;


FlagMapPtr get_flags() {
    FlagMapPtr ptr(new FlagMap());
    char * test_args = getenv("TEST_ARGS");
    BOOST_REQUIRE_MESSAGE(test_args != 0,
                          "TEST_ARGS environment var not defined.");
    if (test_args != 0) {
        ptr->add_from_arg(test_args);
    }
    return ptr;
}

BOOST_AUTO_TEST_CASE(integration_tests)
{
    MySqlApiScope mysql_api_scope;

    FlagValues flags(get_flags());

    CHECK_POINT();

    NewService args;
    args.availability_zone = "Enders";
    args.binary = "nova-guest";
    args.host = "Letterman";
    args.topic = "cereal";

    MySqlConnectionWithDefaultDbPtr connection(
        new MySqlConnectionWithDefaultDb(flags.nova_sql_host(),
            flags.nova_sql_user(), flags.nova_sql_password(),
            flags.nova_sql_database()));
    // Destroy all the matching services.
    {
        CHECK_POINT();
        MySqlPreparedStatementPtr stmt = connection->prepare_statement(
            "DELETE FROM services WHERE  services.binary= ? AND host= ? "
            "AND services.topic = ? AND availability_zone = ?");
        int index = 0;
        stmt->set_string(index ++, args.binary.c_str());
        stmt->set_string(index ++, args.host.c_str());
        stmt->set_string(index ++, args.topic.c_str());
        stmt->set_string(index ++, args.availability_zone.c_str());
        stmt->execute();
    }
    CHECK_POINT();

    ApiPtr api = nova::db::create_api(connection);

    ServicePtr service;

    // Retrieve - should be missing.
    {
        ServicePtr service0 = api->service_get_by_args(args);
        BOOST_CHECK(!service0);
    }

    // Create
    {
        service = api->service_create(args);
        BOOST_CHECK_EQUAL(service->availability_zone, "Enders");
        BOOST_CHECK_EQUAL(service->binary, "nova-guest");
        BOOST_CHECK_EQUAL(service->host, "Letterman");
        BOOST_CHECK_EQUAL(service->topic, "cereal");
        BOOST_CHECK(!!service->disabled);
        if (service->disabled != boost::none) {
            BOOST_CHECK_EQUAL(service->disabled.get(), false);
        }
        BOOST_CHECK_EQUAL(!!service->report_count, 0);
    }


    // Retrieve
    {
        ServicePtr service2 = api->service_get_by_args(args);
        BOOST_CHECK_EQUAL(service2->availability_zone,
                          service->availability_zone);
        BOOST_CHECK_EQUAL(service2->binary, service->binary);
        BOOST_CHECK_EQUAL(service2->host, service->host);
        BOOST_CHECK_EQUAL(service2->topic, service->topic);
        BOOST_CHECK_EQUAL(service2->disabled, service->disabled);
        BOOST_CHECK_EQUAL(service2->report_count, service->report_count);
        BOOST_CHECK_EQUAL(service2->id, service->id);
    }

    // Update
    {
        service->report_count ++;
        service->disabled = optional<bool>(true);
        IsoDateTime new_time;

        api->service_update(*service);

        ServicePtr service2 = api->service_get_by_args(args);
        BOOST_CHECK_EQUAL(service2->availability_zone,
                          service->availability_zone);
        BOOST_CHECK_EQUAL(service2->binary, service->binary);
        BOOST_CHECK_EQUAL(service2->host, service->host);
        BOOST_CHECK_EQUAL(service2->topic, service->topic);
        BOOST_CHECK_EQUAL(!service2->disabled, !service->disabled);
        if (service->disabled) {
            BOOST_CHECK_EQUAL(service2->disabled.get(),
                              service->disabled.get());
        }
        BOOST_CHECK_EQUAL(service2->report_count, service->report_count);
        BOOST_CHECK_EQUAL(service2->id, service->id);
    }
}
