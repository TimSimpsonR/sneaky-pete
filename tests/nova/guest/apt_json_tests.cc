#define BOOST_TEST_MODULE apt_json_tests
#include <boost/test/unit_test.hpp>


/** This tests needs to be linked with apt_json.cc but not apt.cc, as it
 *  defines fake versions of the later. */
#include "nova/guest/apt.h"
#include <boost/foreach.hpp>
#include <boost/optional.hpp>

using nova::JsonObject;
using nova::JsonObjectPtr;
using boost::optional;
using std::string;

namespace {

    struct FakeMethod {

        string package_name;
        int time_out;
        string return_value;

        virtual string operator()(const char * package_name, int time_out) {
            this->package_name = package_name;
            this->time_out = time_out;
            return return_value;
        }
    };

    FakeMethod * install_func = 0;
    FakeMethod * remove_func = 0;
    FakeMethod * version_func = 0;

} // end anonymous namespace

namespace nova { namespace guest { namespace apt {

    void install(const char * package_name, const double time_out) {
        (*install_func)(package_name, time_out);
    }

    void remove(const char * package_name, const double time_out) {
        (*remove_func)(package_name, time_out);
    }

    optional<string> version(const char * package_name, const double time_out) {
        return (*version_func)(package_name, 0);
    }

}   }   }  // end namespace nova::guest::apt


using namespace nova::guest;
using namespace nova::guest::apt;

BOOST_AUTO_TEST_CASE(hi)
{

}

#ifdef IT_IS_THE_FUTURE
BOOST_AUTO_TEST_CASE(install_method)
{
    FakeMethod method;
    method.return_value = "blah";
    install_func = &method;

    AptMessageHandler handler;
    JsonObjectPtr input(new JsonObject(
        "{ 'method':'install', "
        "  'args':{'package_name':'cowsay', 'time_out':462}"
        "}"));
    JsonObjectPtr output = handler.handle_message(input);

    BOOST_CHECK_EQUAL("{ }", output->to_string());
    BOOST_CHECK_EQUAL(method.package_name, "cowsay");
    BOOST_CHECK_EQUAL(method.time_out, 462);
}

BOOST_AUTO_TEST_CASE(remove_method)
{
    FakeMethod method;
    method.return_value = "blah";
    remove_func = &method;

    AptMessageHandler handler;
    JsonObjectPtr input(new JsonObject(
        "{ 'method':'remove', "
        "  'args':{'package_name':'figlet', 'time_out':78200}"
        "}"));
    JsonObjectPtr output = handler.handle_message(input);

    BOOST_CHECK_EQUAL("{ }", output->to_string());
    BOOST_CHECK_EQUAL(method.package_name, "figlet");
    BOOST_CHECK_EQUAL(method.time_out, 78200);
}

BOOST_AUTO_TEST_CASE(version_method)
{
    FakeMethod method;
    method.return_value = "blah";
    version_func = &method;

    AptMessageHandler handler;
    JsonObjectPtr input(new JsonObject(
        "{ 'method':'version', "
        "  'args':{'package_name':'curdsay'}"
        "}"));
    BOOST_CHECK_EQUAL(true, true);
    JsonObjectPtr output = handler.handle_message(input);

    BOOST_CHECK_EQUAL("{ \"version\": \"blah\" }", output->to_string());
    BOOST_CHECK_EQUAL(method.package_name, "curdsay");
}


BOOST_AUTO_TEST_CASE(errors_should_be_caught)
{
    BOOST_CHECK_EQUAL(true, true);
     struct FakeThrowingMethod : public FakeMethod {
        virtual string operator()(const char * package_name, int time_out) {
            throw AptException(AptException::ADMIN_LOCK_ERROR);
        }
    };

    FakeThrowingMethod method;
    method.return_value = "blah";
    install_func = &method;

    AptMessageHandler handler;
    JsonObjectPtr input(new JsonObject(
        "{ 'method':'install', "
        "  'args':{'package_name':'cowsay', 'time_out':462}"
        "}"));
    JsonObjectPtr output = handler.handle_message(input);

    BOOST_CHECK_EQUAL("{ \"error\": \"Cannot complete because admin files are locked.\" }",
                      output->to_string());
}

#endif
