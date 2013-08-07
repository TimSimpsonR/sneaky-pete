#define BOOST_TEST_MODULE nova_guest_HeartBeat_tests
#include <boost/test/unit_test.hpp>

#include "nova/guest/HeartBeat.h"
#include "nova/flags.h"
#include <boost/format.hpp>
#include <boost/optional.hpp>
#include "nova/Log.h"
#include "nova/db/mysql.h"
#include "nova/utils/regex.h"
#include <boost/thread.hpp>
#include "nova/guest/utils.h"

#define CHECK_POINT() BOOST_CHECK_EQUAL(2,2);

using namespace nova::flags;
using boost::format;
using nova::guest::HeartBeat;
using nova::guest::utils::IsoDateTime;
using nova::Log;
using namespace nova::db::mysql;
using namespace nova;
using boost::optional;
using nova::utils::Regex;
using nova::utils::RegexMatchesPtr;
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

optional<string> get_updated_at(MySqlConnectionWithDefaultDbPtr con,
                                const char * instance_id) {
    string query = str(format("SELECT updated_at FROM agent_heartbeats "
                              "WHERE instance_id='%s'")
                       % con->escape_string(instance_id));
    NOVA_LOG_INFO(query.c_str());
    CHECK_POINT();
    MySqlResultSetPtr results = con->query(query.c_str());
    CHECK_POINT();
    if (results->next()) {
        CHECK_POINT();
        return results->get_string(0);
    } else {
        CHECK_POINT();
        return boost::none;
    }
}

BOOST_AUTO_TEST_CASE(integration_tests)
{
    LogApiScope log(LogOptions::simple());
    MySqlApiScope mysql_api_scope;
    FlagValues flags(get_flags());
    const char * GUEST_ID = "testid";

    CHECK_POINT();

    MySqlConnectionWithDefaultDbPtr connection(
        new MySqlConnectionWithDefaultDb(flags.nova_sql_host(),
            flags.nova_sql_user(), flags.nova_sql_password(),
            flags.nova_sql_database()));

    // Destroy all the matching services.
    {
        CHECK_POINT();
        MySqlPreparedStatementPtr stmt = connection->prepare_statement(
            "DELETE FROM agent_heartbeats WHERE instance_id= ?");
        stmt->set_string(0, GUEST_ID);
        stmt->execute();
    }
    CHECK_POINT();

    HeartBeat heart_beat(connection, GUEST_ID);
    CHECK_POINT();
    {
        // Shouldn't find a thing.
        const optional<string> result = get_updated_at(connection, GUEST_ID);
        BOOST_CHECK_EQUAL(boost::none, result);
    }
    // We'll need to make sure the time is in the correct format, i.e.:
    // 2012-05-22 20:19:49
    Regex iso_time_fmt("^[0-9]{4}\\-[0-9]{2}\\-[0-9]{2}\\s[0-9]{2}"
                       "\\:[0-9]{2}\\:[0-9]{2}");

    heart_beat.update();
    IsoDateTime before;

    // Now it had better find something.
    const optional<string> result = get_updated_at(connection, GUEST_ID);
    {
        BOOST_REQUIRE(!!result);
        RegexMatchesPtr matches = iso_time_fmt.match(result.get().c_str());
        BOOST_CHECK(!!matches);
        BOOST_CHECK(matches->exists_at(0));
    }

    // Wait a second before proceeding...

    const int sleep_time = 50;
    int attempts = (1000 / sleep_time) + 2;
    IsoDateTime now;
    do
    {
        attempts --;
        BOOST_REQUIRE_MESSAGE(attempts >= 0, "IsoDateTime is not updating.");
        boost::this_thread::sleep(boost::posix_time::milliseconds(sleep_time));
        now.set_to_now();
        NOVA_LOG_INFO("%s != %s", now.c_str(), before.c_str());
    } while(now == before);


    // Tests that a second call, which executes the update query, doesn't break.
    heart_beat.update();
    const optional<string> result2 = get_updated_at(connection, GUEST_ID);
    {
        // Make sure the update function put a time in there.
        BOOST_REQUIRE(!!result2);
        RegexMatchesPtr matches = iso_time_fmt.match(result2.get().c_str());
        BOOST_CHECK(!!matches);
        BOOST_CHECK(matches->exists_at(0));

        // Make sure it wasn't the same time as before.
        NOVA_LOG_INFO("%s != %s", result.get().c_str(), result2.get().c_str());
        BOOST_CHECK(result.get() != result2.get());
    }
}
