#ifdef gfdhgfhtdjsg
#include "nova/guest/mysql/MySqlDatabase.h"

#include "nova/utils/regex.h"

using boost::none;
using boost::optional;
using nova::utils::Regex;
using nova::utils::RegexMatches;
using nova::utils::RegexMatchesPtr;
using std::string;

namespace nova { namespace guest { namespace mysql {

namespace {
    // char set -> collation types
    struct
    typedef nova::JsonObjectPtr (* MethodPtr)(const MySqlGuestPtr &,

                                                      nova::JsonObjectPtr);
    const string CHAR_SET("utf8");
}

MySqlDatabase::MySqlDatabase(const std::string & name)
: character_set(none), collation(none), name(name)
{
}

const string & MySqlDatabase::get_character_set() const {
    if (!this->character_set) {
        return CHAR_SET;
    } else {
        return this->character_set.get();
    }

}

const string & MySqlDatabase::get_collation() const {
    return collation.get();
}

void MySqlDatabase::set_character_set(const string & value) {
    // check if the given value is a valid character (self.charset)
    //TODO
    this->character_set = value;
}

void MySqlDatabase::set_collation(const optional<const string &> &  value) {
    //
}

void MySqlDatabase::set_name(const string & value) {

}

} } }  // end nova::guest::mysql
#endif
