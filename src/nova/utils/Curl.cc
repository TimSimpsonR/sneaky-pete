#include "Curl.h"
#include <algorithm>
#include <boost/foreach.hpp>
#include <boost/optional.hpp>
#include "nova/Log.h"
#include <boost/format.hpp>
using namespace std;
using namespace boost;


namespace nova { namespace utils {


/**---------------------------------------------------------------------------
 *- CurlScope
 *---------------------------------------------------------------------------*/

CurlScope::CurlScope(){
    curl_global_init(CURL_GLOBAL_ALL);
}

CurlScope::~CurlScope() {
    curl_global_cleanup();
}


/**---------------------------------------------------------------------------
 *- Curl
 *---------------------------------------------------------------------------*/

Curl::Curl()
:   curl(0),
    headers(0)
{
    curl = curl_easy_init();
    if (!curl) {
        throw CurlException(CurlException::CURL_INIT_FAIL);
    }
}

Curl::~Curl()
{
    curl_easy_cleanup(curl);
    if (0 != headers) {
       curl_slist_free_all(headers);
    }
}

void Curl::add_header(const char * header) {
    NOVA_LOG_DEBUG2("Adding header: %s", header);
    headers = curl_slist_append(headers, header);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
}

Curl::HeadersPtr Curl::head(const string & url,
                            const Curl::HttpCodeList & expected_http_codes) {
    set_opt(CURLOPT_NOBODY, 1L);
    set_opt(CURLOPT_HEADER, 1L);
    set_opt(CURLOPT_PUT, 0);
    set_opt(CURLOPT_UPLOAD, 0);
    set_opt(CURLOPT_URL, url.c_str());

    struct CallBack {
        HeadersPtr headers;

        static size_t write_data(void *buffer, size_t size, size_t nmemb,
                                 void *userp) {
            auto self = reinterpret_cast<CallBack *>(userp);
            string text(reinterpret_cast<char *>(buffer), size * nmemb);
            auto pos = text.find(": ");
            if (string::npos != pos) {
                auto itr = text.begin();
                // Convert the key to lower case.
                std::transform(itr, itr + pos, itr, ::tolower);
                Headers & headers = *(self->headers);
                headers[text.substr(0, pos)] = text.substr(pos + 2);
            }
            return (size * nmemb);
        }
    } callback;

    callback.headers.reset(new Headers());

    set_opt(CURLOPT_WRITEFUNCTION, CallBack::write_data);
    set_opt(CURLOPT_WRITEDATA, (void *) &callback);
    perform(expected_http_codes);

    return callback.headers;
}

void Curl::perform(const Curl::HttpCodeList & expected_http_codes,
                   signed int num_retries) {
    CURLcode result = CURLE_FAILED_INIT;
    while(CURLE_OK != (result = curl_easy_perform(curl))) {
        -- num_retries;
        NOVA_LOG_ERROR2("curl_easy_perform() failed (will retry %d times): %s",
                        num_retries, curl_easy_strerror(result));
        if (num_retries <= 0) {
            NOVA_LOG_ERROR("Maximum retries reached!");
            throw CurlException(CurlException::CURL_PERFORM_FAIL);
        }
    }

    long actual_http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &actual_http_code);
    BOOST_FOREACH(const auto code, expected_http_codes) {
        if (code == actual_http_code) {
            return;
        }
    }
    stringstream error_msg;
    error_msg << "Received incorrect HTTP response. Expected codes: [";
    BOOST_FOREACH(const auto code, expected_http_codes) {
        error_msg << code << ", ";
    }
    error_msg << "], Actual code: " << actual_http_code;
    NOVA_LOG_ERROR(error_msg.str().c_str());
    throw CurlException(CurlException::CURL_UNEXPECTED_HTTP_CODE);
}

void Curl::reset() {
    curl_easy_reset(curl);
    if (0 != headers) {
       curl_slist_free_all(headers);
    }
    headers = 0;
}


/**---------------------------------------------------------------------------
 *- CurlException
 *---------------------------------------------------------------------------*/

CurlException::CurlException(Code code) throw()
:   code(code) {
}

CurlException::~CurlException() throw() {
}

const char * CurlException::what() const throw() {
    switch(code) {
        case CURL_INIT_FAIL:
            return "Failure initating curl.";
        case CURL_PERFORM_FAIL:
            return "Failing performing request!";
        case CURL_UNEXPECTED_HTTP_CODE:
            return "Unexpected HTTP code from request!";
        default:
            return "A curl error occurred.";
    }
}


} }  // end namespace nova::utils

