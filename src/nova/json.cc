#include "nova/json.h"


#include <json/json.h>


using std::string;

namespace nova {


namespace {

    /* Throws if json_object is not a JSON array. */
    inline void validate_json_array(json_object * const array_obj,
                                    const JsonException::Code & not_found_ex) {
        if (array_obj == (json_object *)0) {
            throw JsonException(not_found_ex);
        }
        if (json_object_is_type(array_obj, json_type_array) == 0) {
            throw JsonException(JsonException::TYPE_ERROR_NOT_ARRAY);
        }
    }

    /* Turn json_object into an int and return, throw on error. */
    inline int validate_json_int(json_object * const int_obj,
                                 const JsonException::Code & not_found_ex) {
        if (int_obj == (json_object *)0) {
            throw JsonException(not_found_ex);
        }
        if (json_object_is_type(int_obj, json_type_int) == 0) {
            throw JsonException(JsonException::TYPE_ERROR_NOT_INT);
        }
        return json_object_get_int(int_obj);
    }

    /* Throws if json_object is not a JSON object. */
    inline void validate_json_object(json_object * const object_obj,
                                    const JsonException::Code & not_found_ex) {
        if (object_obj == (json_object *)0) {
            throw JsonException(not_found_ex);
        }
        if (json_object_is_type(object_obj, json_type_object) == 0) {
            throw JsonException(JsonException::TYPE_ERROR_NOT_OBJECT);
        }
    }

    /* Get string from json_object, throw on error. */
    inline const char * validate_json_string(json_object * const string_obj,
                                    const JsonException::Code & not_found_ex) {
        if (string_obj == (json_object *)0) {
            throw JsonException(not_found_ex);
        }
        if (json_object_is_type(string_obj, json_type_string) == 0) {
            throw JsonException(JsonException::TYPE_ERROR_NOT_STRING);
        }
        return json_object_get_string(string_obj);
    }

} // end anonymous namespace


JsonException::JsonException(JsonException::Code code) throw()
: code(code)
{
}

JsonException::~JsonException() throw() {
}

const char * JsonException::what() const throw() {
    switch(code) {
        case CTOR_ARGUMENT_IS_NOT_JSON_STRING:
            return "Argument is not a valid JSON string.";
        case CTOR_ARGUMENT_IS_NULL:
            return "Argument can not be null!";
        case CTOR_ARGUMENT_NOT_ARRAY:
            return "Argument must be a JSON array.";
        case CTOR_ARGUMENT_NOT_OBJECT:
            return "Argument must be a JSON object.";
        case INDEX_ERROR:
            return "Invalid index in JSON array.";
        case KEY_ERROR:
            return "No key with the given name found in the JSON object.";
        case TYPE_ERROR_NOT_ARRAY:
            return "The value is not an array.";
        case TYPE_ERROR_NOT_INT:
            return "The value is not an integer.";
        case TYPE_ERROR_NOT_OBJECT:
            return "The value is not an object.";
        case TYPE_ERROR_NOT_STRING:
            return "The value is not a string.";
        default:
            return "An error occurred.";
    }
}

/**---------------------------------------------------------------------------
 *- JsonData
 *---------------------------------------------------------------------------*/

JsonData::JsonData()
: object(0), root(0) {
}

JsonData::~JsonData() {
    if (root != 0) {
        root->reference_count --;
        if (root->reference_count <= 0) {
            json_object_put(root->original);
            delete root;
        }
    }
}

void JsonData::initialize_child(json_object * obj, JsonData::Root * root) {
    this->root = root;
    root->reference_count ++;
    this->object = obj;
}

void JsonData::initialize_root(json_object * obj) {
    this->root = new JsonData::Root();
    root->reference_count = 1;
    root->original = obj;
    this->object = obj;
}

/**---------------------------------------------------------------------------
 *- JsonArray
 *---------------------------------------------------------------------------*/

JsonArray::JsonArray(const char * json_text)
: JsonData(), length(0)
{
    json_object * obj = json_tokener_parse(json_text);
    if (obj == 0) {
        json_object_put(obj);
        throw JsonException(JsonException::CTOR_ARGUMENT_IS_NOT_JSON_STRING);
    }
    check_initial_object(obj, true);
    initialize_root(obj);
    length = json_object_array_length(object);
}

JsonArray::JsonArray(json_object * obj)
: JsonData(), length(0) {
    check_initial_object(obj, false);
    initialize_root(obj);
    length = json_object_array_length(object);
}

JsonArray::JsonArray(json_object * obj, Root * root)
: JsonData(), length(0) {
    check_initial_object(obj, false);
    initialize_child(obj, root);
    length = json_object_array_length(object);
}

json_object * JsonArray::get(int index) const {
    // Shouldn't be necessary but Valgrind pollutes the test logs with
    // messages about invalid reads when "json_object_array_get_idx" is called
    // a bad index. The code otherwise works.
    if (index < 0 || index >= length) {
        throw JsonException(JsonException::INDEX_ERROR);
    }
    return json_object_array_get_idx(object, index);
}

void JsonArray::check_initial_object(json_object * obj, bool owned) {
    if (obj == (json_object*) 0) {
        if (owned) {
            json_object_put(obj);
        }
        throw JsonException(JsonException::CTOR_ARGUMENT_IS_NULL);
    }
    if (json_object_is_type(obj, json_type_array) == 0) {
        if (owned) {
            json_object_put(obj);
        }
        throw JsonException(JsonException::CTOR_ARGUMENT_NOT_ARRAY);
    }
}

JsonArrayPtr JsonArray::get_array(const int index) const {
    json_object * array_obj = get(index);
    validate_json_array(array_obj, JsonException::INDEX_ERROR);
    JsonArrayPtr rtn(new JsonArray(array_obj, root));
    return rtn;
}

int JsonArray::get_int(const int index) const {
    json_object * int_obj = get(index);
    return validate_json_int(int_obj, JsonException::INDEX_ERROR);
}

JsonObjectPtr JsonArray::get_object(const int index) const {
    json_object * object_obj = get(index);
    validate_json_object(object_obj, JsonException::INDEX_ERROR);
    JsonObjectPtr rtn(new JsonObject(object_obj, root));
    return rtn;
}

const char * JsonArray::get_string(const int index) const {
    json_object * string_obj = get(index);
    return validate_json_string(string_obj, JsonException::INDEX_ERROR);
}

void JsonArray::get_string(const int index, std::string & value) const {
    value = get_string(index);
}

/**---------------------------------------------------------------------------
 *- JsonObject
 *---------------------------------------------------------------------------*/

JsonObject::JsonObject(const char * json_text)
: JsonData()
{
    json_object * obj = json_tokener_parse(json_text);
    if (obj == 0) {
        json_object_put(obj);
        throw JsonException(JsonException::CTOR_ARGUMENT_IS_NOT_JSON_STRING);
    }
    check_initial_object(obj, true);
    initialize_root(obj);
}

JsonObject::JsonObject(json_object * obj)
: JsonData()
{
    check_initial_object(obj, false);
    initialize_root(obj);
}

JsonObject::JsonObject(json_object * obj, Root * root)
: JsonData() {
    check_initial_object(obj, false);
    initialize_child(obj, root);
}

void JsonObject::check_initial_object(json_object * obj, bool owned) {
    if (obj == (json_object*) 0) {
        if (owned) {
            json_object_put(obj);
        }
        throw JsonException(JsonException::CTOR_ARGUMENT_IS_NULL);
    }
    if (json_object_is_type(obj, json_type_object) == 0) {
        if (owned) {
            json_object_put(obj);
        }
        throw JsonException(JsonException::CTOR_ARGUMENT_NOT_OBJECT);
    }
}

JsonArrayPtr JsonObject::get_array(const char * key) const {
    json_object * array_obj = json_object_object_get(object, key);
    validate_json_array(array_obj, JsonException::KEY_ERROR);
    JsonArrayPtr rtn(new JsonArray(array_obj, root));
    return rtn;
}

int JsonObject::get_int(const char * key) const {
    json_object * int_obj = json_object_object_get(object, key);
    return validate_json_int(int_obj, JsonException::KEY_ERROR);
}

JsonObjectPtr JsonObject::get_object(const char * key) const {
    json_object * object_obj = json_object_object_get(object, key);
    validate_json_object(object_obj, JsonException::KEY_ERROR);
    JsonObjectPtr rtn(new JsonObject(object_obj, root));
    return rtn;
}

const char * JsonObject::get_string(const char * key) const {
    json_object * string_obj = json_object_object_get(object, key);
    return validate_json_string(string_obj, JsonException::KEY_ERROR);
}

void JsonObject::get_string(const char * key, string & value) const {
    value = get_string(key);
}

const char * JsonObject::to_string() const {
    //return json_object_get_string(object);
    return json_object_to_json_string(object);
}

} // end namespace nova
