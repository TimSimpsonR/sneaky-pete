#define BOOST_TEST_MODULE json_tests
#include <boost/test/unit_test.hpp>
#include <boost/test/unit_test_monitor.hpp>


#include "nova/json.h"
#include <json/json.h>

using nova::JsonArray;
using nova::JsonArrayPtr;
using nova::JsonData;
using nova::JsonDataPtr;
using nova::JsonException;
using nova::JsonObject;
using nova::JsonObjectPtr;
using std::string;


#define CHECKPOINT() BOOST_CHECK_EQUAL(2,2);


// Asserts the given bit of code throws a JsonException instance with the given
// code.
#define CHECK_JSON_EXCEPTION(statement, ex_code) try { \
        statement ; \
        BOOST_FAIL("Should have thrown."); \
    } catch(const JsonException & je) { \
        const char * actual = JsonException::code_to_string(JsonException::ex_code); \
        const char * expected = JsonException::code_to_string(je.code); \
        BOOST_REQUIRE_EQUAL(actual, expected); \
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
    CHECKPOINT();
    // Successfully getting a string
    BOOST_CHECK_EQUAL(2,2);
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
 *- JsonData Tests
 *---------------------------------------------------------------------------*/

BOOST_AUTO_TEST_CASE(from_boolean)
{
    BOOST_CHECK_EQUAL(JsonData::from_boolean(false)->to_string(), "false");
    BOOST_CHECK_EQUAL(JsonData::from_boolean(true)->to_string(), "true");
}

BOOST_AUTO_TEST_CASE(from_number)
{
    BOOST_CHECK_EQUAL(JsonData::from_number(0)->to_string(), "0");
    BOOST_CHECK_EQUAL(JsonData::from_number(423)->to_string(), "423");
}

BOOST_AUTO_TEST_CASE(from_null)
{
    BOOST_CHECK_EQUAL(JsonData::from_null()->to_string(), "null");
}

BOOST_AUTO_TEST_CASE(from_string)
{
    BOOST_CHECK_EQUAL(JsonData::from_string("hello")->to_string(), "\"hello\"");
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
}

BOOST_AUTO_TEST_CASE(must_not_allow_null_in_array_constructor)
{
    CHECK_JSON_EXCEPTION({ JsonArray array((json_object *)0); },
                         CTOR_ARGUMENT_IS_NOT_JSON_STRING);
}

BOOST_AUTO_TEST_CASE(get_values)
{
    JsonObject object("{ 'string':'abcde', 'int':42 }");
    test_object(object);
}

BOOST_AUTO_TEST_CASE(getting_an_array_from_within_an_array) {
    CHECKPOINT();
    JsonArray array_array(json_tokener_parse(
        "[[42,'two',3048], '2', [42,'two',3048]]"));
    CHECKPOINT();
    JsonArrayPtr array0 = array_array.get_array(0);
    test_json_array(*array0);

    CHECKPOINT();
    const char * two = array_array.get_string(1);
    BOOST_CHECK_EQUAL(two, "2");

    CHECKPOINT();
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
    CHECKPOINT();

    JsonArray array_array(json_tokener_parse(
        "[[42,'two',3048], {'string':'abcde', 'int':42 }]"));

    CHECKPOINT();

    JsonObjectPtr obj = array_array.get_object(1);
    CHECKPOINT();
    test_object(*obj);

    CHECKPOINT();

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
}

BOOST_AUTO_TEST_CASE(must_not_allow_null_in_constructor)
{
    CHECK_JSON_EXCEPTION({ JsonObject object((json_object *) 0); },
                         CTOR_ARGUMENT_IS_NOT_JSON_STRING);
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
    CHECKPOINT();
    JsonObject object(json_tokener_parse(
        "{ 'type':'return', 'response':{'string':'abcde', 'int':42 }}"));
    CHECKPOINT();
    JsonObjectPtr obj2 = object.get_object("response");
    test_object(*obj2); // Run the same tests as above.

    CHECK_JSON_EXCEPTION({ object.get_object("blah"); }, KEY_ERROR);

    CHECK_JSON_EXCEPTION({ object.get_object("type"); },
                     TYPE_ERROR_NOT_OBJECT);
}

BOOST_AUTO_TEST_CASE(has_item_works)
{
    JsonObject object(json_tokener_parse(
        "{ 'hovercraft': 'full', 'eels': null }"));
    BOOST_CHECK_EQUAL(object.has_item("hovercraft"), true);
    BOOST_CHECK_EQUAL(object.has_item("tobacconist"), false);
    BOOST_CHECK_EQUAL(object.has_item("eels"), false);
}

BOOST_AUTO_TEST_CASE(get_int_or_default_works)
{
    JsonObject object(json_tokener_parse(
        "{ 'bacon': 4, 'lettuce': 2, 'tomato': 1 }"));
    BOOST_CHECK_EQUAL(object.get_int_or_default("tomato", 0), 1);
    BOOST_CHECK_EQUAL(object.get_int_or_default("tuna", 5), 5);
}
