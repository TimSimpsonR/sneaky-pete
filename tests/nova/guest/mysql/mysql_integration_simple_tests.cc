#define BOOST_TEST_MODULE Integration_Tests_MySQL_Simple
#include <boost/test/unit_test.hpp>

#include "nova/guest/apt.h"
#include "nova/flags.h"
#include <boost/foreach.hpp>
#include <boost/format.hpp>
#include <boost/optional.hpp>
#include "nova/Log.h"
#include <memory>
#include "nova/db/mysql.h"
#include "nova/db/MySqlConfigReader.h"
#include "nova/utils/regex.h"
#include <stdlib.h>

using nova::Log;
using nova::LogApiScope;
using nova::LogOptions;
using nova::utils::Regex;
using nova::utils::RegexMatchesPtr;

struct GlobalFixture {

    LogApiScope log;

    GlobalFixture()
    : log(LogOptions::simple()) {
    }

};

BOOST_GLOBAL_FIXTURE(GlobalFixture);

//#include "nova/"
#define CHECK_POINT() BOOST_CHECK_EQUAL(2,2);
#define CHECK_EXCEPTION(statement, ex_code) try { \
        statement ; \
        BOOST_FAIL("Should have thrown."); \
    } catch(const MySqlException & mse) { \
        const char * actual = MySqlException::code_to_string(MySqlException::ex_code); \
        const char * expected = MySqlException::code_to_string(mse.code); \
        BOOST_REQUIRE_EQUAL(actual, expected); \
    }

#define ACCEPT_EXCEPTION(statement, ex_code) try { \
        statement ; \
    } catch(const MySqlException & mse) { \
        const char * actual = MySqlException::code_to_string(MySqlException::ex_code); \
        const char * expected = MySqlException::code_to_string(mse.code); \
        BOOST_REQUIRE_EQUAL(actual, expected); \
    }

using namespace nova::flags;
using boost::format;
using nova::Log;
using namespace nova::db::mysql;
using boost::optional;
using std::string;
using std::stringstream;

const char * ADMIN_USER_NAME = "os_admin";
const double INITIAL_USER_COUNT = 4;
const double TIME_OUT = 60;
const char * URI = "localhost:5672";

FlagMapPtr get_flags() {
    //int argc = boost::unit_test::framework::master_test_suite().argc;
    //char ** argv = boost::unit_test::framework::master_test_suite().argv;
    //BOOST_REQUIRE_EQUAL(2, argc);
    FlagMapPtr ptr(new FlagMap());
    char * test_args = getenv("TEST_ARGS");
    BOOST_REQUIRE_MESSAGE(test_args != 0, "TEST_ARGS environment var not defined.");
    if (test_args != 0) {
        ptr->add_from_arg(test_args);
    }
    //return FlagMap::create_from_args(argc, argv, true);
    return ptr;
}

BOOST_AUTO_TEST_CASE(config_reader)
{
    stringstream str;
    str << "[blah]" << std::endl;
    str << "user = wrong" << std::endl;
    str << "password = wrong" << std::endl;
    str << "[nothing]" << std::endl;
    str << "socket = 3484:3==3" << std::endl;
    str << "fjf = fgdjkgfldg" << std::endl;
    str << "fj!!!f = fgdjk!!gfldg" << std::endl;
    str << "garbage!!" << std::endl;
    str << "[client]" << std::endl;
    str << "user = right_user" << std::endl;
    str << "password = right_password" << std::endl;
    str << "[blah2]" << std::endl;
    str << "user = wrong2" << std::endl;
    str << "password = wrong2" << std::endl;

    optional<MySqlConfigCreds> result = get_creds_from_file(str);
    BOOST_REQUIRE(!!result);
    BOOST_REQUIRE_EQUAL("right_user", result.get().user);
    BOOST_REQUIRE_EQUAL("right_password", result.get().password);
}

BOOST_AUTO_TEST_CASE(integration_tests)
{
    MySqlApiScope mysql_api_scope;

    FlagValues flags(get_flags());

    const char * sql_connection_string = flags.sql_connection();
    //--sql_connection=mysql://nova:novapass@10.0.4.15/nova
    Regex regex("mysql:\\/\\/(\\w+):(\\w+)@([0-9\\.]+)\\/(\\w+)");
    RegexMatchesPtr matches = regex.match(sql_connection_string);
    BOOST_REQUIRE(matches);
    for (int i = 0; i < 4; i ++) {
        BOOST_REQUIRE(matches->exists_at(i));
    }
    const string user(matches->get(1));
    const string password(matches->get(2));
    const string host(matches->get(3));
    const string database(matches->get(4));

    MySqlConnectionWithDefaultDb connection(host.c_str(), user.c_str(),
                                            password.c_str(), database.c_str());

    CHECK_EXCEPTION({ connection.query("use not_real"); }, QUERY_FAILED);

    // It's acceptable if database does not exist to throw here.
    ACCEPT_EXCEPTION({connection.query("DROP DATABASE simple_test"); },
                     QUERY_FAILED);

    connection.query("CREATE DATABASE simple_test");

    connection.query("use simple_test");

    connection.query("CREATE TABLE test_table(name VARCHAR(50) NULL, age INT)");

    // Demonstrating how result set works on queries that return nothing.
    {
        MySqlResultSetPtr result = connection.query(
            "INSERT INTO test_table VALUES(\"hub_cap\", 273)"
        );
        BOOST_CHECK_EQUAL(result->get_field_count(), 0);
        // These queries return nothing, so they must throw.
        CHECK_EXCEPTION({ result->get_string(0); }, FIELD_INDEX_OUT_OF_BOUNDS);
        BOOST_CHECK_EQUAL(result->next(), false);
        BOOST_CHECK_EQUAL(result->get_row_count(), 0);
    }
    connection.query("INSERT INTO test_table VALUES(NULL, 0)");
    connection.query("INSERT INTO test_table VALUES(\"grapex\", -9500)");

    {
        MySqlResultSetPtr result = connection.query(
            "SELECT * FROM test_table ORDER BY name");
        BOOST_CHECK_EQUAL(result->get_field_count(), 2);
        // These queries return nothing, so they must throw.
        CHECK_EXCEPTION({ result->get_string(0); }, QUERY_RESULT_SET_NOT_STARTED);
        BOOST_CHECK_EQUAL(result->get_row_count(), 0);

        // row 0 (not real)
        BOOST_CHECK_EQUAL(result->get_row_count(), 0);
        BOOST_CHECK_EQUAL(result->get_field_count(), 2);
        CHECK_EXCEPTION({ result->get_string(0); }, QUERY_RESULT_SET_NOT_STARTED);
        CHECK_EXCEPTION({ result->get_string(1); }, QUERY_RESULT_SET_NOT_STARTED);
        CHECK_EXCEPTION({ result->get_string(2); }, FIELD_INDEX_OUT_OF_BOUNDS);

        // row 1
        BOOST_REQUIRE_EQUAL(result->next(), true);
        BOOST_CHECK_EQUAL(result->get_row_count(), 1);
        BOOST_CHECK_EQUAL(result->get_field_count(), 2);
        BOOST_CHECK_EQUAL(result->get_string(0), boost::none);
        BOOST_CHECK_EQUAL(result->get_string(1).get(), "0");
        CHECK_EXCEPTION({ result->get_string(2); }, FIELD_INDEX_OUT_OF_BOUNDS);

        // row 2
        BOOST_REQUIRE_EQUAL(result->next(), true);
        BOOST_CHECK_EQUAL(result->get_row_count(), 2);
        BOOST_CHECK_EQUAL(result->get_field_count(), 2);
        BOOST_CHECK_EQUAL(result->get_string(0).get(), "grapex");
        BOOST_CHECK_EQUAL(result->get_string(1).get(), "-9500");
        CHECK_EXCEPTION({ result->get_string(-1); }, FIELD_INDEX_OUT_OF_BOUNDS);

        // row 3
        BOOST_REQUIRE_EQUAL(result->next(), true);
        BOOST_CHECK_EQUAL(result->get_row_count(), 3);
        BOOST_CHECK_EQUAL(result->get_field_count(), 2);
        BOOST_CHECK_EQUAL(result->get_string(0).get(), "hub_cap");
        BOOST_CHECK_EQUAL(result->get_string(1).get(), "273");
        CHECK_EXCEPTION({ result->get_string(2); }, FIELD_INDEX_OUT_OF_BOUNDS);

        // No more
        BOOST_REQUIRE_EQUAL(result->next(), false);
        BOOST_CHECK_EQUAL(result->get_row_count(), 3);
        BOOST_CHECK_EQUAL(result->get_field_count(), 2);
        CHECK_EXCEPTION({ result->get_string(0); }, QUERY_RESULT_SET_FINISHED);
        CHECK_EXCEPTION({ result->get_string(1); }, QUERY_RESULT_SET_FINISHED);
        CHECK_EXCEPTION({ result->get_string(2); }, FIELD_INDEX_OUT_OF_BOUNDS);

        // no more
        CHECK_POINT();
        BOOST_CHECK_EQUAL(result->next(), false);
        CHECK_POINT();
    }


    // Now its on to prepared statements.
    //TODO: These aren't working. The special result set type can't convert
    // every fetched argument to string, and using the same result set type
    // as Query causes it to return nothing but empty strings. :(
    {
        MySqlPreparedStatementPtr stmt = connection.prepare_statement(
            "SELECT * FROM test_table WHERE name= ?");

        BOOST_CHECK_EQUAL(stmt->get_parameter_count(), 1);
        CHECK_EXCEPTION({ stmt->set_string(-1, "2"); },
                        PARAMETER_INDEX_OUT_OF_BOUNDS);
        CHECK_EXCEPTION({ stmt->set_string(1, "2"); },
                        PARAMETER_INDEX_OUT_OF_BOUNDS);
        stmt->set_string(0, "hub_cap");

        stmt->execute();
/*
        // row 0 (not real)
        BOOST_CHECK_EQUAL(result->get_row_count(), 0);
        BOOST_CHECK_EQUAL(result->get_field_count(), 2);
        CHECK_EXCEPTION({ result->get_string(0); }, QUERY_RESULT_SET_NOT_STARTED); //RESULT_SET_NOT_STARTED);
        CHECK_EXCEPTION({ result->get_string(1); }, QUERY_RESULT_SET_NOT_STARTED); // RESULT_SET_NOT_STARTED);
        CHECK_EXCEPTION({ result->get_string(2); }, FIELD_INDEX_OUT_OF_BOUNDS); //RESULT_INDEX_OUT_OF_BOUNDS);

        // row 1
        BOOST_REQUIRE_EQUAL(result->next(), true);
        BOOST_CHECK_EQUAL(result->get_row_count(), 1);
        BOOST_CHECK_EQUAL(result->get_field_count(), 2);
        //BOOST_CHECK_EQUAL(result->get_string(0).get(), "hub_cap");
        //BOOST_CHECK_EQUAL(result->get_string(1).get(), "273");
        CHECK_EXCEPTION({ result->get_string(2); }, FIELD_INDEX_OUT_OF_BOUNDS); //RESULT_INDEX_OUT_OF_BOUNDS);

         // No more
        BOOST_REQUIRE_EQUAL(result->next(), false);
        BOOST_CHECK_EQUAL(result->get_row_count(), 1);
        BOOST_CHECK_EQUAL(result->get_field_count(), 2);
        CHECK_EXCEPTION({ result->get_string(0); }, QUERY_RESULT_SET_FINISHED); //RESULT_SET_FINISHED);
        CHECK_EXCEPTION({ result->get_string(1); }, QUERY_RESULT_SET_FINISHED); //RESULT_SET_FINISHED);
        CHECK_EXCEPTION({ result->get_string(2); }, FIELD_INDEX_OUT_OF_BOUNDS); //RESULT_INDEX_OUT_OF_BOUNDS);

        // no more
        CHECK_POINT();
        BOOST_CHECK_EQUAL(result->next(), false);
        CHECK_POINT();
*/
        //stmt->
        //MySqlResultSetPtr result = connection.query(

    }
}
