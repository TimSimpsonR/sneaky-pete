#define BOOST_TEST_MODULE SwiftTests
#include <boost/test/unit_test.hpp>

#include <boost/foreach.hpp>
#include <boost/format.hpp>
#include "nova/Log.h"
#include <string>
#include "nova/utils/swift.h"
#include <vector>

// Confirm the macros works everywhere by not using nova::Log.
using boost::format;
using boost::optional;
using std::string;
using std::vector;

using namespace nova;
using namespace nova::utils::swift;

struct SwiftFileInfoFixture {

    SwiftFileInfo file;
    LogApiScope log;

    SwiftFileInfoFixture()
    : file("http://local.com/cloud", "container", "filename"),
      log(LogOptions::simple())
    {
    }

};

BOOST_FIXTURE_TEST_SUITE(swift_file_info_tests, SwiftFileInfoFixture);

BOOST_AUTO_TEST_CASE(container_url)
{
    BOOST_CHECK_EQUAL(file.container_url(), "http://local.com/cloud/container");
}

BOOST_AUTO_TEST_CASE(formatted_url)
{
    BOOST_CHECK_EQUAL(file.formatted_url(34),
                      "http://local.com/cloud/container/filename_00000034");
}

BOOST_AUTO_TEST_CASE(manifest_url)
{
    BOOST_CHECK_EQUAL(file.manifest_url(),
                      "http://local.com/cloud/container/filename");
}

BOOST_AUTO_TEST_CASE(prefix_header)
{
    BOOST_CHECK_EQUAL(file.prefix_header(),
                      "X-Object-Manifest: container/filename_");
}

BOOST_AUTO_TEST_CASE(segment_header)
{
    BOOST_CHECK_EQUAL(file.segment_header(36),
                      "X-Object-Meta-Segments: 36");
}

BOOST_AUTO_TEST_SUITE_END();
