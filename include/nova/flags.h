#ifndef __NOVA_FLAGS_H
#define __NOVA_FLAGS_H

#include <boost/optional.hpp>
#include <boost/smart_ptr.hpp>
#include <map>
#include <string>

namespace nova { namespace flags {

class FlagException : public std::exception {

        public:
            enum Code {
                DUPLICATE_FLAG_VALUE,
                FILE_NOT_FOUND,
                KEY_NOT_FOUND,
                NO_EQUAL_SIGN,
                NO_STARTING_DASHES,
                PATTERN_GROUP_NOT_MATCHED,
                PATTERN_NOT_MATCHED
            };

            FlagException(const Code code, const char * line) throw();

            virtual ~FlagException() throw();

            static const char * code_to_string(Code code) throw();

            virtual const char * what() const throw();

            const Code code;

            char msg[256];

            bool use_msg;
};

class FlagValues;
typedef boost::shared_ptr<FlagValues> FlagValuesPtr;

/** This is a bumpkin version of Nova flags that can't do fancy things such
 *  as read vaues with spaces in them.  */
class FlagValues {

    public:

        // Adds a line in the form "--name=value"
        void add_from_arg(const char * arg, bool ignore_mismatch=false);

        void add_from_arg(const std::string & arg, bool ignore_mismatch=false);

        // Opens up a file and adds everything.
        void add_from_file(const char * file_path);

        static FlagValuesPtr create_from_args(size_t count, char** argv,
                                              bool ignore_mismatch=false);

        static FlagValuesPtr create_from_file(const char* argv);

        const char * get(const char * const name);

        int get_as_int(const char * const name);

        void get_sql_connection(std::string & host, std::string & user,
                                std::string & password,
                                boost::optional<std::string> & database);

    private:

        typedef std::map<std::string, std::string> Map;

        Map map;
};


} } // end nova::flags

#endif
