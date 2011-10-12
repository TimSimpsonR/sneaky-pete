#include "nova/utils/regex.h"

#include <regex.h>
#include <stdexcept>

namespace nova { namespace utils {

/**---------------------------------------------------------------------------
 *- Regex
 *---------------------------------------------------------------------------*/

Regex::Regex(const char * pattern) {
    regcomp(&regex, pattern, REG_EXTENDED);
}

Regex::~Regex() {
    regfree(&regex);
}

RegexMatchesPtr Regex::match(const char * line, size_t max_matches) const {
    regmatch_t * matches = new regmatch_t[max_matches];
    if (regexec(&regex, line, max_matches, matches, 0) == 0) {
        RegexMatchesPtr ptr(new RegexMatches(line, matches, max_matches));
        return ptr;
    } else {
        delete[] matches;
        return RegexMatchesPtr();
    }
}


/**---------------------------------------------------------------------------
 *- RegexMatches
 *---------------------------------------------------------------------------*/

RegexMatches::RegexMatches(const char * line, regmatch_t * matches, size_t size)
: line(line), matches(matches), nmatch(size) {
}

RegexMatches::~RegexMatches() {
    delete[] matches;
}

bool RegexMatches::exists_at(size_t index) const {
    if (index < nmatch) {
        const regoff_t & start_index = matches[index].rm_so;
        return start_index >= 0;
    }
    return false;
}

std::string RegexMatches::get(size_t index) const {
    if (!exists_at(index)) {
        static const std::string error_msg("No match exists at given index.");
        throw std::out_of_range(error_msg);
    }
    const regoff_t & start_index = matches[index].rm_so;
    const regoff_t & end_index = matches[index].rm_eo;
    const regoff_t match_size = end_index - start_index;
    return line.substr(start_index, match_size);
}

std::string RegexMatches::original_line() const {
    return line;
}

} }  // end of nova::utils namespace
