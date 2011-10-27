#ifndef __NOVA_FLAGS_H
#define __NOVA_FLAGS_H

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
                NO_STARTING_DASHES
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
        void add_from_arg(const char * arg);

        void add_from_arg(const std::string & arg);

        // Opens up a file and adds everything.
        void add_from_file(const char * file_path);

        static FlagValuesPtr create_from_args(size_t count, const char* argv[]);

        static FlagValuesPtr create_from_file(const char* argv);

        const char * get(const char * const name);

        const int get_int(const char * const name);

    private:

        typedef std::map<std::string, std::string> Map;

        Map map;
};


} } // end nova::flags

#endif
