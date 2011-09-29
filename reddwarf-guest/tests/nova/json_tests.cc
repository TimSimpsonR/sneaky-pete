#define BOOST_TEST_MODULE json_tests
#include <boost/test/unit_test.hpp>
#include <boost/test/unit_test_monitor.hpp>


#include "nova/json.h"
#include <json/json.h>

using nova::JsonArray;
using nova::JsonArrayPtr;
using nova::JsonException;
using nova::JsonObject;
using nova::JsonObjectPtr;
using std::string;

/* This code registers the JsonException type so that if its thrown Boost.Test
 * will show us the error message instead of just printing 'std::exception'. */
void translate_json_exception(const JsonException & je) {
    std::stringstream msg;
    msg << "JsonException:" << je.what();
    BOOST_ERROR(msg.str());
}

struct FixtureConfig {

    FixtureConfig() {
        using boost::unit_test::unit_test_monitor;
        unit_test_monitor.register_exception_translator<JsonException>(
            &translate_json_exception);
    }

    ~FixtureConfig() {
    }
};

BOOST_GLOBAL_FIXTURE( FixtureConfig );

// Asserts the given bit of code throws a JsonException instance with the given
// code.
#define CHECK_JSON_EXCEPTION(statement, ex_code) try { \
        statement ; \
        BOOST_FAIL("Should have thrown."); \
    } catch(const JsonException & je) { \
        BOOST_REQUIRE_EQUAL(je.code, JsonException::ex_code); \
    }

// Tests the array [42,"two", 3048].
void test_json_array(JsonArray & array) {
    // Try to get an integer with a bad key
    CHECK_JSON_EXCEPTION({ array.get_int(-1); }, INDEX_ERROR);

    CHECK_JSON_EXCEPTION({ array.get_int(3); }, INDEX_ERROR);

    int one = array.get_int(0);
    BOOST_CHECK_EQUAL(one, 42);

    // Try to get a string with a bad key

    CHECK_JSON_EXCEPTION({ array.get_string(-1); }, INDEX_ERROR);
    CHECK_JSON_EXCEPTION({ array.get_string(3); }, INDEX_ERROR);

    const char * two = array.get_string(1);
    BOOST_CHECK_EQUAL(two, "two");

    int three = array.get_int(2);
    BOOST_CHECK_EQUAL(three, 3048);

    // Try to get an integer that's really a string.
    CHECK_JSON_EXCEPTION({ array.get_int(1); }, TYPE_ERROR_NOT_INT);

    // Try to get a string that's really an int.
    CHECK_JSON_EXCEPTION({ array.get_string(0); }, TYPE_ERROR_NOT_STRING);
}

// Tests the object {"string":"abcde", "int":42}
void test_object(JsonObject & object) {
    // Successfully getting a string
    {
        string string_value;
        object.get_string("string", string_value);
        BOOST_CHECK_EQUAL(string_value, "abcde");
    }

    // Attempting to get a string with a bad key.
    CHECK_JSON_EXCEPTION({ string string_value;
                           object.get_string("blah", string_value); },
                         KEY_ERROR);

    // Attempting to get an integer as a string.
    CHECK_JSON_EXCEPTION({ string string_value;
                           object.get_string("int", string_value); },
                        TYPE_ERROR_NOT_STRING);

    // Successfully getting an int.
    {
        int int_value = object.get_int("int");
        BOOST_CHECK_EQUAL(int_value, 42);
    }

    // Attempting to get an int that doesn't exit.
    CHECK_JSON_EXCEPTION({ object.get_int("blah"); }, KEY_ERROR);

    // Attempting to get an int as a string.
    CHECK_JSON_EXCEPTION({ object.get_int("string"); }, TYPE_ERROR_NOT_INT);
}

/**---------------------------------------------------------------------------
 *- JsonArray Tests
 *---------------------------------------------------------------------------*/

BOOST_AUTO_TEST_CASE(creating_an_array_via_string)
{
    JsonArray numbers("[42,'two',3048]");
    test_json_array(numbers);
}

BOOST_AUTO_TEST_CASE(creating_an_array_via_json_object)
{
    JsonArray numbers(json_tokener_parse("[42,'two',3048]"));
    test_json_array(numbers);
}

BOOST_AUTO_TEST_CASE(must_not_allow_non_array_type_in_constructor)
{
    json_object * object = json_tokener_parse("{'hello':'hi'}");
    CHECK_JSON_EXCEPTION({ JsonArray array(object); },
                         CTOR_ARGUMENT_NOT_ARRAY);
    json_object_put(object);
}

BOOST_AUTO_TEST_CASE(must_not_allow_null_in_array_constructor)
{
    CHECK_JSON_EXCEPTION({ JsonArray array((json_object *)0); },
                         CTOR_ARGUMENT_IS_NULL);
}

BOOST_AUTO_TEST_CASE(get_values)
{
    JsonObject object("{ 'string':'abcde', 'int':42 }");
    test_object(object);
}

BOOST_AUTO_TEST_CASE(getting_an_array_from_within_an_array) {
    JsonArray array_array(json_tokener_parse(
        "[[42,'two',3048], '2', [42,'two',3048]]"));
    JsonArrayPtr array0 = array_array.get_array(0);
    test_json_array(*array0);

    const char * two = array_array.get_string(1);
    BOOST_CHECK_EQUAL(two, "2");

    JsonArrayPtr array2 = array_array.get_array(2);
    test_json_array(*array2);

    CHECK_JSON_EXCEPTION({array_array.get_array(-1);},
                         INDEX_ERROR);
    CHECK_JSON_EXCEPTION({array_array.get_array(3);},
                         INDEX_ERROR);
    CHECK_JSON_EXCEPTION({array_array.get_array(1);},
                         TYPE_ERROR_NOT_ARRAY);
}

BOOST_AUTO_TEST_CASE(getting_an_object_from_within_an_array) {
    JsonArray array_array(json_tokener_parse(
        "[[42,'two',3048], {'string':'abcde', 'int':42 }]"));

    JsonObjectPtr obj = array_array.get_object(1);
    test_object(*obj);

    CHECK_JSON_EXCEPTION({array_array.get_object(-1);},
                         INDEX_ERROR);
    CHECK_JSON_EXCEPTION({array_array.get_object(2);},
                         INDEX_ERROR);
    CHECK_JSON_EXCEPTION({array_array.get_object(0);},
                         TYPE_ERROR_NOT_OBJECT);
}

/**---------------------------------------------------------------------------
 *- JsonObject Tests
 *---------------------------------------------------------------------------*/

BOOST_AUTO_TEST_CASE(must_not_allow_non_object_type_in_constructor)
{
    json_object * original = json_object_new_string("hello");
    CHECK_JSON_EXCEPTION({ JsonObject object(original); },
                         CTOR_ARGUMENT_NOT_OBJECT);

    // If the constructor throws, the memory isn't freed.
    json_object_put(original);
}

BOOST_AUTO_TEST_CASE(must_not_allow_null_in_constructor)
{
    CHECK_JSON_EXCEPTION({ JsonObject object((json_object *) 0); },
                         CTOR_ARGUMENT_IS_NULL);
}

BOOST_AUTO_TEST_CASE(creating_malformed_object)
{
    CHECK_JSON_EXCEPTION({ JsonObject object("{52}"); },
                         CTOR_ARGUMENT_IS_NOT_JSON_STRING);

    CHECK_JSON_EXCEPTION({ JsonObject object("[1,2, 4]"); },
                         CTOR_ARGUMENT_NOT_OBJECT);
}

BOOST_AUTO_TEST_CASE(getting_arrays_from_inside_an_object)
{
    JsonObject object(json_tokener_parse(
        "{ 'type':'return', 'response':[42,'two',3048]}"));

    JsonArrayPtr array = object.get_array("response");
    test_json_array(*array);
    CHECK_JSON_EXCEPTION({object.get_array("blah");},
                         KEY_ERROR);
    CHECK_JSON_EXCEPTION({object.get_array("type");},
                         TYPE_ERROR_NOT_ARRAY);
}

BOOST_AUTO_TEST_CASE(getting_values_from_inside_an_object)
{
    JsonObject object(json_tokener_parse(
        "{ 'type':'return', 'response':{'string':'abcde', 'int':42 }}"));

    JsonObjectPtr obj2 = object.get_object("response");
    test_object(*obj2); // Run the same tests as above.

    CHECK_JSON_EXCEPTION({ object.get_object("blah"); }, KEY_ERROR);

    CHECK_JSON_EXCEPTION({ object.get_object("type"); },
                         TYPE_ERROR_NOT_OBJECT);
}
