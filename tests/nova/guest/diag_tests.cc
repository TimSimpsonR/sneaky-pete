#define BOOST_TEST_MODULE diag_tests
#include <boost/test/unit_test.hpp>

#include "nova/guest/diagnostics.h"
#include "nova/Log.h"
#include "nova/guest/version.h"
#include "nova/utils/regex.h"
#include <fstream>
#include <unistd.h>

using boost::optional;
using namespace nova;
using namespace nova::guest;
using namespace nova::guest::diagnostics;
using std::string;
using std::stringstream;

struct GlobalFixture {

    LogApiScope log;

    GlobalFixture()
    : log(LogOptions::simple()) {
    }

};

BOOST_GLOBAL_FIXTURE(GlobalFixture);

/**---------------------------------------------------------------------------
 *- Diagnostics Tests
 *---------------------------------------------------------------------------*/

BOOST_AUTO_TEST_CASE(get_something_back_from_diagnostics)
{
    diagnostics::Interrogator guest;
    DiagInfoPtr diags = guest.get_diagnostics();
    BOOST_REQUIRE(diags.get() != 0);
    BOOST_REQUIRE(diags->count("VmSize") > 0);
    BOOST_REQUIRE(diags->count("VmPeak") > 0);
    BOOST_REQUIRE(diags->count("VmRSS") > 0);
    BOOST_REQUIRE(diags->count("VmHWM") > 0);
    BOOST_REQUIRE(diags->count("Threads") > 0);
    BOOST_REQUIRE_EQUAL((*diags)["version"], NOVA_GUEST_CURRENT_VERSION);
}
