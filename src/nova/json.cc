#include "pch.hpp"
#include "nova/json.h"
#include <json/json.h>
using boost::lexical_cast;
using boost::optional;
using std::string;
using std::vector;

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
    inline int validate_json_bool(json_object * const bool_obj,
                                 const JsonException::Code & not_found_ex) {
        if (bool_obj == (json_object *)0) {
            throw JsonException(not_found_ex);
        }
        if (json_object_is_type(bool_obj, json_type_boolean) == 0) {
            throw JsonException(JsonException::TYPE_ERROR_NOT_BOOL);
        }
        return json_object_get_int(bool_obj);
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

    /* Get int from json_object, throw on error. */
    inline const int get_json_int_or_default(json_object * const int_obj,
                                                   const int default_value) {
        if (int_obj == (json_object *)0) {
            return default_value;
        }
        if (json_object_is_type(int_obj, json_type_int) == 0) {
            return default_value;
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

    /* Get string from json_object, throw on error. */
    inline const char * get_json_string_or_default(json_object * const string_obj,
                                                   const char * default_value) {
        if (string_obj == (json_object *)0) {
            return default_value;
        }
        if (json_object_is_type(string_obj, json_type_string) == 0) {
            return default_value;
        }
        return json_object_get_string(string_obj);
    }

} // end anonymous namespace


JsonException::JsonException(JsonException::Code code) throw()
: code(code) {
}

JsonException::~JsonException() throw() {
}

const char * JsonException::code_to_string(Code code) {
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
        case TYPE_ERROR_NOT_BOOL:
            return "The value is not a bool.";
        case TYPE_ERROR_NOT_INT:
            return "The value is not an integer.";
        case TYPE_ERROR_NOT_OBJECT:
            return "The value is not an object.";
        case TYPE_ERROR_NOT_POSITIVE_INT:
            return "The value is not a positive integer.";
        case TYPE_ERROR_NOT_STRING:
            return "The value is not a string.";
        case TYPE_INCORRECT:
            return "The JSON type was different than expected.";
        default:
            return "An error occurred.";
    }
}

const char * JsonException::what() const throw() {
    return code_to_string(code);
}

/**---------------------------------------------------------------------------
 *- json_string
 *---------------------------------------------------------------------------*/

std::string json_string(const char * text) {
    return JsonData::from_string(text)->to_string();
}


/**---------------------------------------------------------------------------
 *- JsonData
 *---------------------------------------------------------------------------*/

JsonData::Visitor::~Visitor() {
}

JsonData::JsonData()
: object(0), root(0) {
}

JsonData::JsonData(json_object * obj)
: object(0), root(0) {
    if (obj == 0) {
        json_object_put(obj);
        throw JsonException(JsonException::CTOR_ARGUMENT_IS_NOT_JSON_STRING);
    }
    set_root(obj);
}

JsonData::JsonData(json_object * obj, int type_i)
: object(0), root(0) {
    json_type type = (json_type) type_i;
    if (type != json_type_null) {
        if (obj == 0) {
            json_object_put(obj);
            throw JsonException(JsonException::CTOR_ARGUMENT_IS_NOT_JSON_STRING);
        }
        if (json_object_get_type(obj) != type) {
            json_object_put(obj);
            throw JsonException(JsonException::TYPE_INCORRECT);
        }
    } else {
        if (obj != 0) {
            throw JsonException(JsonException::TYPE_INCORRECT);
        }
    }
    set_root(obj);
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

void JsonData::check_initial_object(bool owned, json_object * obj,
                                    int type_i,
                                    JsonException::Code exception_code) {
    if (obj == (json_object*) 0) {
        if (owned) {
            json_object_put(obj);
        }
        throw JsonException(JsonException::CTOR_ARGUMENT_IS_NOT_JSON_STRING);
    }
    json_type type = (json_type) type_i;
    if (json_object_is_type(obj, type) == 0) {
        if (owned) {
            json_object_put(obj);
        }
        throw JsonException(exception_code);
    }
}

JsonDataPtr JsonData::create_child(json_object * obj) const {
    JsonDataPtr rtn(new JsonData());
    rtn->initialize_child_no_check(obj, root);
    return rtn;
}

JsonDataPtr JsonData::from_boolean(bool value) {
    json_object * obj = json_object_new_boolean(value);
    JsonDataPtr ptr(new JsonData(obj, json_type_boolean));
    return ptr;
}

JsonDataPtr JsonData::from_number(int number) {
    json_object * obj = json_object_new_int(number);
    JsonDataPtr ptr(new JsonData(obj, json_type_int));
    return ptr;
}

JsonDataPtr JsonData::from_null() {
    json_object * obj = json_tokener_parse("null");
    JsonDataPtr ptr(new JsonData(obj, json_type_null));
    return ptr;
}

JsonDataPtr JsonData::from_string(const char * text) {
    json_object * obj = json_object_new_string(text);
    JsonDataPtr ptr(new JsonData(obj, json_type_string));
    return ptr;
}


void JsonData::initialize_child(json_object * obj, int type_i,
                                JsonException::Code exception_code,
                                Root * root) {
    json_type type = (json_type) type_i;
    check_initial_object(false, obj, type, exception_code);
    initialize_child_no_check(obj, root);
}

void JsonData::initialize_child_no_check(json_object * obj, Root * root) {
    this->root = root;
    root->reference_count ++;
    this->object = obj;
}

void JsonData::initialize_root(json_object * obj, int type_i,
                               JsonException::Code exception_code) {
    json_type type = (json_type) type_i;
    check_initial_object(true, obj, type, exception_code);

    set_root(obj);
}

void JsonData::set_root(json_object * obj) {
    this->root = new JsonData::Root();
    root->reference_count = 1;
    root->original = obj;
    this->object = obj;
}

const char * JsonData::to_string() const {
    if (object == 0) {
        return "null";
    }
    return json_object_to_json_string(object);
}


void JsonData::visit(JsonData::Visitor & visitor) const {
    // Strangely, json_object_get_type cannot safely be called with an
    // argument that is null (even though other json.h functions will return
    // null pointers). So we need to check for null explicitly first.
    const json_type type = NULL == object ? json_type_null
                                          : json_object_get_type(object);
    switch(type) {
        case json_type_boolean: {
            visitor.for_boolean(0 != json_object_get_int(object));
            break;
        }
        case json_type_double: {
            visitor.for_double(json_object_get_double(object));
            break;
        }
        case json_type_int: {
            visitor.for_int(json_object_get_int(object));
            break;
        }
        case json_type_object: {
            JsonObjectPtr value(new JsonObject(object, root));
            visitor.for_object(value);
            break;
        }
        case json_type_array: {
            JsonArrayPtr value(new JsonArray(object, root));
            visitor.for_array(value);
            break;
        }
        case json_type_string: {
            const char * value = json_object_get_string(object);
            visitor.for_string(value);
            break;
        }
        case json_type_null:
        default:
            visitor.for_null();
            break;
    }
}


/**---------------------------------------------------------------------------
 *- JsonArray
 *---------------------------------------------------------------------------*/

JsonArray::JsonArray(const char * json_text)
: JsonData(), length(0) {
    initialize(json_text);
}

JsonArray::JsonArray(const JsonArrayBuilder & array)
: JsonData(), length(0) {
    string s = lexical_cast<string>(array);
    initialize(s.c_str());
}
JsonArray::JsonArray(json_object * obj)
: JsonData(), length(0) {
    initialize_root(obj, json_type_array,
                    JsonException::CTOR_ARGUMENT_NOT_ARRAY);
    length = json_object_array_length(object);
}

JsonArray::JsonArray(json_object * obj, Root * root)
: JsonData(), length(0) {
    initialize_child(obj, json_type_array,
                     JsonException::CTOR_ARGUMENT_NOT_ARRAY, root);
    length = json_object_array_length(object);
}

JsonArray::~JsonArray() {

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

JsonDataPtr JsonArray::get_any(const int index) const {
    json_object * any_obj = get(index);
    return create_child(any_obj);
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

void JsonArray::initialize(const char * json_text) {
    json_object * obj = json_tokener_parse(json_text);
    if (obj == 0) {
        json_object_put(obj);
        throw JsonException(JsonException::CTOR_ARGUMENT_IS_NOT_JSON_STRING);
    }
    initialize_root(obj, json_type_array,
                    JsonException::CTOR_ARGUMENT_NOT_ARRAY);
    length = json_object_array_length(object);
}

vector<string> JsonArray::to_string_vector() const {
    vector<string> container;
    for (int index = 0; index < get_length(); ++ index) {
        string s(get_any(index)->to_string());
        container.push_back(s);
    }
    return container;
}

/**---------------------------------------------------------------------------
 *- JsonObject
 *---------------------------------------------------------------------------*/

JsonObject::JsonObject(const char * json_text)
: JsonData() {
    initialize(json_text);
}

JsonObject::JsonObject(const JsonObjectBuilder & obj)
: JsonData() {
    std::string s = boost::lexical_cast<std::string>(obj);
    initialize(s.c_str());
}

JsonObject::JsonObject(json_object * obj)
: JsonData() {
    initialize_root(obj, json_type_object,
                    JsonException::CTOR_ARGUMENT_NOT_OBJECT);
}

JsonObject::JsonObject(json_object * obj, Root * root)
: JsonData() {
    initialize_child(obj, json_type_object,
                     JsonException::CTOR_ARGUMENT_NOT_OBJECT, root);
}

JsonObject::~JsonObject() {

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

JsonDataPtr JsonObject::get_any(const char * key) const {
    json_object * any_obj = json_object_object_get(object, key);
    return create_child(any_obj);
}


JsonArrayPtr JsonObject::get_array(const char * key) const {
    json_object * array_obj = json_object_object_get(object, key);
    validate_json_array(array_obj, JsonException::KEY_ERROR);
    JsonArrayPtr rtn(new JsonArray(array_obj, root));
    return rtn;
}

bool JsonObject::get_bool(const char * key) const {
    json_object * bool_obj = json_object_object_get(object, key);
    return validate_json_bool(bool_obj, JsonException::KEY_ERROR);
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

optional<bool> JsonObject::get_optional_bool(const char * key) const {
    json_object * bool_obj = json_object_object_get(object, key);
    if (bool_obj == (json_object *)0) {
        return boost::none;
    } else {
        bool rtn = validate_json_bool(bool_obj, JsonException::KEY_ERROR);
        return optional<bool>(rtn);
    }
}

JsonObjectPtr JsonObject::get_optional_object(const char * key) const {
    json_object * object_obj = json_object_object_get(object, key);
    if ((json_object *)0 == object_obj) {
        return JsonObjectPtr();
    } else {
        validate_json_object(object_obj, JsonException::KEY_ERROR);
        JsonObjectPtr rtn(new JsonObject(object_obj, root));
        return rtn;
    }
}

optional<string> JsonObject::get_optional_string(const char * key) const {
    json_object * string_obj = json_object_object_get(object, key);
    if (string_obj == (json_object *)0) {
        return boost::none;
    } else {
        string rtn = validate_json_string(string_obj, JsonException::KEY_ERROR);
        return optional<string>(rtn);
    }
}

JsonObjectPtr JsonObject::get_object_or_empty(const char * key) const {
    json_object * object_obj = json_object_object_get(object, key);
    if (object_obj == (json_object *)0) {
        JsonObjectPtr rtn(new JsonObject("{}"));
        return rtn;
    } else {
        validate_json_object(object_obj, JsonException::INDEX_ERROR);
        JsonObjectPtr rtn(new JsonObject(object_obj, root));
        return rtn;
    }
}


const char * JsonObject::get_string(const char * key) const {
    json_object * string_obj = json_object_object_get(object, key);
    return validate_json_string(string_obj, JsonException::KEY_ERROR);
}

void JsonObject::get_string(const char * key, string & value) const {
    value = get_string(key);
}

int JsonObject::get_int_or_default(const char * key,
                                   const int default_value) const {
    json_object * int_obj = json_object_object_get(object, key);
    return get_json_int_or_default(int_obj, default_value);
}

unsigned int JsonObject::get_positive_int(const char * key) const {
    int any_int = get_int(key);
    if (any_int < 0) {
        throw JsonException(JsonException::TYPE_ERROR_NOT_POSITIVE_INT);
    }
    return (unsigned int) any_int;
}

const char * JsonObject::get_string_or_default(const char * key,
                                               const char * default_value) const {
    json_object * string_obj = json_object_object_get(object, key);
    return get_json_string_or_default(string_obj, default_value);
}

bool JsonObject::has_item(const char * key) const {
    json_object * object_obj = json_object_object_get(object, key);
    return (object_obj != (json_object *)0);
}

void JsonObject::initialize(const char * json_text) {
    json_object * obj = json_tokener_parse(json_text);
    if (obj == 0) {
        json_object_put(obj);
        throw JsonException(JsonException::CTOR_ARGUMENT_IS_NOT_JSON_STRING);
    }
    initialize_root(obj, json_type_object,
                    JsonException::CTOR_ARGUMENT_NOT_OBJECT);
}

void JsonObject::iterate(JsonObject::Iterator & itr) {
    // This is a macro defined in the JSON lib we're using.
    json_object_object_foreach(object, key_object, value_object) {
        const char * key = key_object;
        auto value = create_child(value_object);
        itr.for_each(key, *value);
    }
}



/**---------------------------------------------------------------------------
 *- JsonDataWriter
 *---------------------------------------------------------------------------*/

JsonDataBuilder::JsonDataBuilder()
:   msg(),
    seen_comma(false) {
}

JsonDataBuilder::JsonDataBuilder(const JsonDataBuilder & other)
:   msg(),
    seen_comma(other.seen_comma) {
    msg.str(other.msg.str());
}

void JsonDataBuilder::assign(const JsonDataBuilder & other) {
    this->msg.str(other.msg.str());
    this->seen_comma = other.seen_comma;
}

JsonDataBuilder::~JsonDataBuilder() {
}



/**---------------------------------------------------------------------------
 *- JsonArrayWriter
 *---------------------------------------------------------------------------*/

JsonArrayBuilder::JsonArrayBuilder()
:   JsonDataBuilder() {
}

JsonArrayBuilder::JsonArrayBuilder(const JsonArrayBuilder & other)
:   JsonDataBuilder(other) {
}

JsonArrayBuilder & JsonArrayBuilder::operator=(
    const JsonArrayBuilder & other) {
    this->assign(other);
    return *this;
}

JsonArrayBuilder::~JsonArrayBuilder() {
}


std::ostream & operator<<(std::ostream & source,
                          const JsonArrayBuilder & obj) {
    source << "[ " << obj.msg.str() << " ]";
    return source;
}


/**---------------------------------------------------------------------------
 *- JsonObjectWriter
 *---------------------------------------------------------------------------*/

JsonObjectBuilder::JsonObjectBuilder()
:   JsonDataBuilder() {
}

JsonObjectBuilder::JsonObjectBuilder(const JsonObjectBuilder & other)
:   JsonDataBuilder(other) {
}

JsonObjectBuilder & JsonObjectBuilder::operator=(
    const JsonObjectBuilder & other) {
    this->assign(other);
    return *this;
}

JsonObjectBuilder::~JsonObjectBuilder() {
}


std::ostream & operator<<(std::ostream & source,
                          const JsonObjectBuilder & obj) {
    source << "{ " << obj.msg.str() << " }";
    return source;
}

} // end namespace nova
