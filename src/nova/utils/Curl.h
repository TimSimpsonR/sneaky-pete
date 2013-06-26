#ifndef _NOVA_UTILS_CURL_H
#define _NOVA_UTILS_CURL_H

#include <sys/stat.h>
#include <curl/curl.h>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <map>
#include <boost/smart_ptr.hpp>
#include <stdio.h>
#include <vector>
#include <unistd.h>
#include <boost/utility.hpp>


namespace nova { namespace utils {


class CurlScope : boost::noncopyable {
public:
    CurlScope();
    ~CurlScope();
};


class Curl : boost::noncopyable {
public:

    typedef std::map<std::string, std::string> Headers;
    typedef boost::shared_ptr<Headers> HeadersPtr;
    typedef std::vector<long> HttpCodeList;

    Curl();

    ~Curl();

    void add_header(const char * header);

    inline CURL * get_curl() {
        return curl;
    }

    HeadersPtr head(const std::string & url,
                    const HttpCodeList & expected_http_codes);

    void perform(const Curl::HttpCodeList & expected_http_code,
                 signed int num_retries = 0);

    void reset();

    template<typename T>
    void set_opt(CURLoption option, T parameter) {
        ::curl_easy_setopt(curl, option, parameter);
    }

private:
    CURL * curl;
    curl_slist * headers;
};


class CurlException : public std::exception {

    public:
        enum Code {
            CURL_INIT_FAIL,
            CURL_PERFORM_FAIL,
            CURL_UNEXPECTED_HTTP_CODE
        };

        CurlException(Code code) throw();

        virtual ~CurlException() throw();

        virtual const char * what() const throw();

    private:
        Code code;
};


} }  // end namespace nova::utils


#endif
