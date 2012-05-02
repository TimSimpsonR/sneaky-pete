#define BOOST_TEST_MODULE diag_tests
#include <boost/test/unit_test.hpp>

#include "nova/guest/diagnostics.h"
#include "nova/Log.h"
#include "nova_guest_version.hpp"
#include "nova/utils/regex.h"
#include <fstream>
#include <unistd.h>
#include <sstream>

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
    BOOST_CHECK(diags.get() != 0);
    BOOST_CHECK_EQUAL(diags->fd_size, 64);
    BOOST_CHECK(diags->vm_peak > 0 && diags->vm_peak < 25000);
    BOOST_CHECK(diags->vm_rss > 0 && diags->vm_rss < 5000);
    BOOST_CHECK(diags->vm_hwm > 0 && diags->vm_hwm < 5000);
    BOOST_CHECK_EQUAL(diags->threads, 1);
    BOOST_CHECK_EQUAL(diags->version, NOVA_GUEST_CURRENT_VERSION);
}


/** Filesystem stats test */
BOOST_AUTO_TEST_CASE(get_filesystem_stats)
{
    unsigned long mb = 1024*1024;
    diagnostics::Interrogator guest;
    FileSystemStatsPtr stats = guest.get_filesystem_stats("/home");
    std::ostringstream oss;
    oss << "Block Size: " << stats->block_size << ", ";
    oss << "Total Blocks: " << stats->total_blocks << ", ";
    oss << "Free Blocks: " << stats->free_blocks << "\n";
    oss << "Total: " << stats->total << ", Used: " << stats->used << ", Free: " << stats->free;
    NOVA_LOG_DEBUG(oss.str().c_str());
    BOOST_CHECK(stats->block_size > 0);
    BOOST_CHECK(stats->total_blocks > 0);
    BOOST_CHECK(stats->free_blocks > 0);
    BOOST_CHECK(stats->total > mb);
    BOOST_CHECK(stats->free > mb);
    BOOST_CHECK(stats->used > mb);
}
