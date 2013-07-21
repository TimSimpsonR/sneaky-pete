#ifndef _NOVA_UTILS_REGEX_H
#define _NOVA_UTILS_REGEX_H

#include <boost/smart_ptr.hpp>
#include <regex.h>
#include <string>
#include <boost/utility.hpp>


namespace nova { namespace utils {

class RegexMatches;
typedef boost::shared_ptr<RegexMatches> RegexMatchesPtr;

class Regex : boost::noncopyable {
    public:
        Regex(const char * pattern);
        ~Regex();

        bool has_match(const char * line) const {
            return !!match(line);
        }

        bool has_match(const std::string & line) const {
            return has_match(line.c_str());
        }

        RegexMatchesPtr match(const char * line, size_t max_matches=5) const;

    private:
        Regex(const Regex &);
        Regex & operator = (const Regex &);

        regex_t regex;
};

class RegexMatches : boost::noncopyable {
    public:
        RegexMatches(const char * line, regmatch_t * matches, size_t size);
        ~RegexMatches();

        bool exists_at(size_t index) const;

        std::string get(size_t index) const;

        std::string original_line() const;

    private:
        RegexMatches(const RegexMatches &);
        RegexMatches & operator = (const RegexMatches &);

        std::string line;
        regmatch_t * matches;
        size_t nmatch;
};


class RegexException : public std::exception {

    public:
        RegexException() throw();

        virtual ~RegexException() throw();

        virtual const char * what() const throw();
};

} }  // end namespace

#endif
