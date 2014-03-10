#ifndef __NOVA_JSON_H
#define __NOVA_JSON_H

#include <boost/optional.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/intrusive_ptr.hpp>
#include <boost/shared_ptr.hpp>
#include <string>
#include <vector>
#include <boost/utility.hpp>


struct json_object;



namespace nova {

    /*-----------------------------------------------------------------------
     * Builder methods / classes - use these to construct Json data.
     *-----------------------------------------------------------------------*/

    std::string json_string(const char * text);

    inline std::string json_string(const std::string & text) {
        return json_string((const char *) text.c_str());
    }

    class JsonArrayBuilder;

    class JsonObjectBuilder;

    class JsonDataBuilder {

        protected:
            JsonDataBuilder();

            JsonDataBuilder(const JsonDataBuilder & other);

            ~JsonDataBuilder();

            void add_value(const int value) {
                add_unescaped_value(value);
            }

            void add_value(const unsigned int value) {
                add_unescaped_value(value);
            }

            void add_value(const float value) {
                add_unescaped_value(value);
            }

            void add_value(const double value) {
                add_unescaped_value(value);
            }

            void add_value(const bool value) {
                add_unescaped_value(value);
            }

            void add_value(const JsonArrayBuilder & value) {
                add_unescaped_value(value);
            }

            void add_value(const JsonObjectBuilder & value) {
                add_unescaped_value(value);
            }

            void add_value(const boost::none_t & value) {
                add_unescaped_value("null");
            }

            template<typename T>
            void add_value(const boost::optional<T> & value) {
                if (!value) {
                    add_unescaped_value("null");
                }
                else {
                    add_value(value.get());
                }
            }

            template<typename T>
            void add_value(const boost::shared_ptr<T> & value) {
                if (!value) {
                    add_unescaped_value("null");
                }
                else {
                    add_value(*value);
                }
            }

            template<typename T>
            void add_value(const boost::intrusive_ptr<T> & value) {
                if (!value)
                {
                    add_unescaped_value("null");
                }
                else
                {
                    add_value(*value);
                }
            }

            template<typename T>
            void add_value(const T & value) {
                add_string_value(value);
            }

            // Convert to a string
            template<typename T>
            void add_string_value(const T & value) {
                const auto str_value = boost::lexical_cast<std::string>(value);
                add_unescaped_value(json_string(str_value));
            }

            // For values that don't need quotes and aren't strings.
            template<typename T>
            void add_unescaped_value(const T & value) {
                msg << value;
            }

            void assign(const JsonDataBuilder & other);

            std::stringstream msg;

            bool seen_comma;
    };

    class JsonArrayBuilder : public JsonDataBuilder
    {
        public:
            JsonArrayBuilder();

            JsonArrayBuilder(const JsonArrayBuilder & other);

            JsonArrayBuilder & operator=(const JsonArrayBuilder & other);

            ~JsonArrayBuilder();

            template<typename T>
            void add(const T & value) {
                if (seen_comma) {
                    msg << ", ";
                }
                add_value(value);
                seen_comma = true;
            }

            // Allows for multiple additions at once.
            template<typename HeadType, typename... TailTypes>
            void add(const HeadType & value, const TailTypes... tail) {
                add(value);
                add(tail...);
            }

            template<typename T>
            void add_unescaped(const T & value) {
                if (seen_comma) {
                    msg << ", ";
                }
                add_unescaped_value(value);
                seen_comma = true;
            }

        friend std::ostream & operator<<(std::ostream & source,
                                         const JsonArrayBuilder & obj);
    };

    std::ostream & operator<<(std::ostream & source,
                              const JsonArrayBuilder & obj);

    template<typename... Types>
    JsonArrayBuilder json_array(const Types... args) {
        JsonArrayBuilder array;
        array.add(args...);
        return array;
    }


    class JsonObjectBuilder : public JsonDataBuilder
    {
        public:
            JsonObjectBuilder();

            JsonObjectBuilder(const JsonObjectBuilder & other);

            JsonObjectBuilder & operator=(const JsonObjectBuilder & other);

            ~JsonObjectBuilder();

            template<typename T>
            void add(const char * name, const T & value) {
                if (seen_comma) {
                    msg << ", ";
                }
                msg << json_string(name) << " : ";
                add_value(value);
                seen_comma = true;
            }

            // Allows for multiple additions at once.
            template<typename HeadType, typename... TailTypes>
            void add(const char * name, const HeadType & value,
                     const TailTypes... tail) {
                add(name, value);
                add(tail...);
            }

            template<typename T>
            void add_unescaped(const char * name, const T & value) {
                if (seen_comma) {
                    msg << ", ";
                }
                msg << json_string(name) << " : ";
                add_unescaped_value(value);
                seen_comma = true;
            }

        friend std::ostream & operator<<(std::ostream & source,
                                         const JsonObjectBuilder & obj);
    };

    std::ostream & operator<<(std::ostream & source,
                              const JsonObjectBuilder & obj);

    /** Creates a new Json object constructed from a series of arguments.
     *  Would have called it "json_object" but I'd have to jump through
     *  hoops since the struct of the c library we're using has the same
     *  name. */
    template<typename... Types>
    JsonObjectBuilder json_obj(const Types... args) {
        JsonObjectBuilder obj;
        obj.add(args...);
        return obj;
    }

    /*------------------------------------------------------------------------
     * Json accessor methods / classes - use these to read Json data.
     *-----------------------------------------------------------------------*/

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
                TYPE_ERROR_NOT_BOOL,
                TYPE_ERROR_NOT_INT,
                TYPE_ERROR_NOT_OBJECT,
                TYPE_ERROR_NOT_POSITIVE_INT,
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


    class JsonData : boost::noncopyable  {
        public:
            class Visitor {
            public:
                virtual ~Visitor();
                virtual void for_boolean(bool value) = 0;
                virtual void for_string(const char * value) = 0;
                virtual void for_double(double value) = 0;
                virtual void for_int(int value) = 0;
                virtual void for_null() = 0;
                virtual void for_object(JsonObjectPtr object) = 0;
                virtual void for_array(JsonArrayPtr array) = 0;
            };

            JsonData(json_object * obj);

            virtual ~JsonData();

            static JsonDataPtr from_boolean(bool value);

            static JsonDataPtr from_number(int number);

            static JsonDataPtr from_null();

            static JsonDataPtr from_string(const char * text);

            // Escapes string. Adds quotes to beginning and end.
            static std::string json_string(const char * text);

            static inline std::string json_string(const std::string & text) {
                return json_string((const char *) text.c_str());
            }

            void visit(Visitor & visit) const;

            const char * to_string() const;

        protected:
            struct Root {
                int reference_count;
                json_object * original;
            };

            JsonData();

            JsonDataPtr create_child(json_object * obj) const;

            // Validates that json_object is of the given type and sets obj.
            // If anything fails it will *not* free the json_object and throw.
            // Sets root without validation.
            void initialize_child(
                json_object * obj, int type,
                JsonException::Code exception_code,
                Root * root);

            // Validates that json_object is of the given type and sets root.
            // If anything fails it will free the json_object and throw.
            void initialize_root(json_object * obj, int type,
                                 JsonException::Code exception_code);

            json_object * object;
            Root * root;

        private:
            JsonData(const JsonData &);
            JsonData & operator = (const JsonData &);

            JsonData(json_object * obj, int type);


            // Validates that obj is not null, is of the given type. If not it
            // frees it if "owned" is set to true and throws.
            void check_initial_object(bool owned, json_object * obj, int type,
                                      JsonException::Code exception_code);

            // Sets this object's object and root fields to the given values,
            // and increases the reference count on root.
            void initialize_child_no_check(json_object * obj, Root * root);

            void set_root(json_object * obj);
    };


    class JsonArray : public JsonData {
        friend class JsonData;
        friend class JsonObject;

        public:
            JsonArray(const char * json_text);

            JsonArray(json_object * obj);

            JsonArray(const JsonArrayBuilder & array);

            virtual ~JsonArray();

            JsonDataPtr get_any(const int index) const;

            JsonArrayPtr get_array(const int index) const;

            int get_int(const int index) const;

            inline int get_length() const {
                return length;
            }

            JsonObjectPtr get_object(const int index) const;

            const char * get_string(const int index) const;

            void get_string(const int index, std::string & value) const;

            std::vector<std::string> to_string_vector() const;

        protected:
            JsonArray(json_object * obj, Root * root);

            void check_initial_object(json_object * obj, bool owned);

            json_object * get(int index) const;

        private:
            // Do not want.
            JsonArray(const JsonArray &);
            JsonArray & operator = (const JsonArray &);

            void initialize(const char * json_text);

            int length;
    };


    /* Holds onto json_object references and manages memory for us.
     * This class also has helper functions for value retrieval that throw
     * exceptions if values aren't found. */
    class JsonObject : public JsonData {
        friend class JsonArray;
        friend class JsonData;

        public:
            class Iterator {
            public:
                virtual void for_each(const char * key,
                                      const JsonData & value) = 0;
            };

            JsonObject(const char * json_text);

            JsonObject(json_object * obj);

            JsonObject(const JsonObjectBuilder & obj);

            virtual ~JsonObject();

            JsonDataPtr get_any(const char * key) const;

            JsonArrayPtr get_array(const char * key) const;

            bool get_bool(const char * key) const;

            int get_int(const char * key) const;

            JsonObjectPtr get_object(const char * key) const;

            JsonObjectPtr get_object_or_empty(const char * key) const;

            boost::optional<bool> get_optional_bool(const char * key) const;

            JsonObjectPtr get_optional_object(const char * key) const;

            boost::optional<std::string> get_optional_string(const char * key)
                const;

            const char * get_string(const char * key) const;

            void get_string(const char * key, std::string & value) const;

            int get_int_or_default(const char * key,
                                               const int default_value) const;

            unsigned int get_positive_int(const char * key) const;

            const char * get_string_or_default(const char * key,
                                               const char * default_value) const;

            /* NB: Also returns false if the value for the given key is present, but null. */
            bool has_item(const char * key) const;

            void iterate(Iterator & itr);
        protected:

            JsonObject(json_object * obj, Root * root);

            void check_initial_object(json_object * obj, bool owned);

        private:
            // Do not want.
            JsonObject(const JsonObject &);
            JsonObject & operator = (const JsonObject &);

            void initialize(const char * json_text);

    };

} // end namespace


#endif
