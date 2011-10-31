#ifndef __NOVA_JSON_H
#define __NOVA_JSON_H


#include <boost/shared_ptr.hpp>
#include <string>


struct json_object;



namespace nova {

    class JsonException : public std::exception {
        public:
            enum Code {
                CTOR_ARGUMENT_IS_NOT_JSON_STRING,
                CTOR_ARGUMENT_NOT_ARRAY,
                CTOR_ARGUMENT_NOT_OBJECT,
                CTOR_ARGUMENT_IS_NULL,
                INDEX_ERROR,
                KEY_ERROR,
                TYPE_ERROR_NOT_ARRAY,
                TYPE_ERROR_NOT_INT,
                TYPE_ERROR_NOT_OBJECT,
                TYPE_ERROR_NOT_STRING,
                TYPE_INCORRECT
            };

            JsonException(Code code) throw();

            virtual ~JsonException() throw();

            static const char * code_to_string(Code code);

            virtual const char * what() const throw();

            const Code code;
    };

    class JsonArray;

    class JsonData;

    class JsonObject;

    typedef boost::shared_ptr<JsonData> JsonDataPtr;

    typedef boost::shared_ptr<JsonArray> JsonArrayPtr;

    typedef boost::shared_ptr<JsonObject> JsonObjectPtr;


    class JsonData {

        public:
            JsonData(json_object * obj);

            virtual ~JsonData();

            static JsonDataPtr from_boolean(bool value);

            static JsonDataPtr from_number(int number);

            static JsonDataPtr from_null();

            static JsonDataPtr from_string(const char * text);

            const char * to_string() const;

        protected:
            struct Root {
                int reference_count;
                json_object * original;
            };

            JsonData();

            // Validates that json_object is of the given type and sets obj.
            // If anything fails it will *not* free the json_object and throw.
            // Sets root without validation.
            void initialize_child(json_object * obj, int type,
                                  JsonException::Code exception_code,
                                  Root * root);

            // Validates that json_object is of the given type and sets root.
            // If anything fails it will free the json_object and throw.
            void initialize_root(json_object * obj, int type,
                                 JsonException::Code exception_code);

            json_object * object;
            Root * root;

        private:
            JsonData(json_object * obj, int type);

            // Validates that obj is not null, is of the given type. If not it
            // frees it if "owned" is set to true and throws.
            void check_initial_object(bool owned, json_object * obj, int type,
                                      JsonException::Code exception_code);

            void set_root(json_object * obj);
    };

    class JsonArray : public JsonData {
        friend class JsonObject;

        public:
            JsonArray(const char * json_text);

            JsonArray(json_object * obj);

            virtual ~JsonArray();

            JsonArrayPtr get_array(const int index) const;

            int get_int(const int index) const;

            JsonObjectPtr get_object(const int index) const;

            const char * get_string(const int index) const;

            void get_string(const int index, std::string & value) const;

        protected:
            JsonArray(json_object * obj, Root * root);

            void check_initial_object(json_object * obj, bool owned);

            json_object * get(int index) const;

        private:

            int length;
    };


    /* Holds onto json_object references and manages memory for us.
     * This class also has helper functions for value retrieval that throw
     * exceptions if values aren't found. */
    class JsonObject : public JsonData {
        friend class JsonArray;

        public:
            JsonObject(const char * json_text);

            JsonObject(json_object * obj);

            virtual ~JsonObject();

            JsonArrayPtr get_array(const char * key) const;

            int get_int(const char * key) const;

            JsonObjectPtr get_object(const char * key) const;

            const char * get_string(const char * key) const;

            void get_string(const char * key, std::string & value) const;

        protected:

            JsonObject(json_object * obj, Root * root);

            void check_initial_object(json_object * obj, bool owned);

        private:

    };

} // end namespace

#endif
