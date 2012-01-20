#define BOOST_TEST_MODULE diag_tests
#include <boost/test/unit_test.hpp>

#include "nova/guest/diagnostics.h"
#include "nova/Log.h"
#include "nova/utils/regex.h"
#include <fstream>
#include <unistd.h>

using boost::optional;
using namespace nova;
using namespace nova::guest;
using namespace nova::guest::diagnostics;
using std::string;
using std::stringstream;

const double TIME_OUT = 60;
const bool USE_SUDO = true;

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
    diagnostics::Interrogator guest(USE_SUDO);
    double timeout = 30;
    DiagInfoPtr diags = guest.get_diagnostics(timeout);
    BOOST_TEST_MESSAGE( "Passing in the timeout value of " << timeout );
    // stringstream msg;
    // msg << "DiagInfoPtr.get() = " << diags.get();
    // BOOST_FAIL(msg.str().c_str());
    BOOST_REQUIRE(diags.get() != 0);
}
