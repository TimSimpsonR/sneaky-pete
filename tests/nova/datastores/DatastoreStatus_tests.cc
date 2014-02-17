#define BOOST_TEST_MODULE nova_datastores_DatastoreStatus_tests
#include <boost/test/unit_test.hpp>

#include "nova/flags.h"
#include <boost/optional.hpp>
#include "nova/Log.h"
#include "nova/datastores/DatastoreStatus.h"
#include "nova/rpc/Sender.h"
#include "nova/guest/utils.h"

using namespace nova::flags;
using nova::guest::utils::IsoDateTime;
using nova::Log;
using nova::LogOptions;
using nova::datastores::DatastoreStatus;
using boost::optional;
using nova::rpc::ResilientSenderPtr;
using std::string;

namespace nova { namespace guest { namespace mysql {


struct GlobalFixture {

    LogApiScope log;

    GlobalFixture()
    : log(LogOptions::simple())
    {
    }

    ~GlobalFixture() {
    }
};

BOOST_GLOBAL_FIXTURE(GlobalFixture);


/** Make a slight variant which doesn't call Conductor or actually try to
 *  figure out the status of a locally running app. */
class TestDatastoreStatus : public DatastoreStatus {
    public:
        TestDatastoreStatus(bool is_installed)
        : DatastoreStatus(ResilientSenderPtr(), is_installed),
          last_sent_status(),
          next_determined_status(DatastoreStatus::RUNNING) {
        }

        optional<Status> last_sent_status;

        Status next_determined_status;

    protected:
        virtual Status determine_actual_status() const {
            return next_determined_status;
        }

        virtual void set_status(Status status) {
            last_sent_status = status;
        }
};


struct DatastoreStatusInstalledTestsFixture {
    TestDatastoreStatus updater;

    DatastoreStatusInstalledTestsFixture()
    :   updater(true) {
    }
};


BOOST_FIXTURE_TEST_SUITE(DatastoreStatusTestSuite,
                         DatastoreStatusInstalledTestsFixture);

BOOST_AUTO_TEST_CASE(typical_happy_instance_path) {
    // Sneaky wakes up to find a running instance. With no "prepare" message
    // lined up, it soons begin merrily reporting this to Conductor.

    BOOST_REQUIRE_EQUAL(!updater.last_sent_status, true);

    updater.next_determined_status = DatastoreStatus::RUNNING;
    updater.update();  // Conductor will be called.
    BOOST_REQUIRE_EQUAL(!updater.last_sent_status, false);
    BOOST_REQUIRE_EQUAL(updater.last_sent_status, DatastoreStatus::RUNNING);
}

BOOST_AUTO_TEST_SUITE_END();


struct DatastoreStatusUninstalledTestsFixture {
    TestDatastoreStatus updater;

    DatastoreStatusUninstalledTestsFixture()
    :   updater(false) {
    }
};


BOOST_FIXTURE_TEST_SUITE(DatastoreStatusTestSuite2,
                         DatastoreStatusUninstalledTestsFixture);


BOOST_AUTO_TEST_CASE(When_in_build_state) {
    updater.begin_install();
    // Right after this is called, conductor is given the status of BUILDING.
    BOOST_REQUIRE_EQUAL(updater.is_installed(), false);
    BOOST_REQUIRE_EQUAL(!updater.last_sent_status, false);
    BOOST_REQUIRE_EQUAL(updater.last_sent_status.get(),
                        DatastoreStatus::BUILDING);

    // This next part simulates a very frequent case: we're installing the
    // app, and so if it's status is literally determine it will be SHUTDOWN
    // because it isn't yet running. However the status reported must remain
    // BUILDING.
    updater.next_determined_status = DatastoreStatus::SHUTDOWN;
    updater.update(); // Shouldn't call Conductor in install mode.
    BOOST_REQUIRE_EQUAL(updater.is_installed(), false);
    BOOST_REQUIRE_EQUAL(!updater.last_sent_status, false);
    BOOST_REQUIRE_EQUAL(updater.last_sent_status.get(),
                      DatastoreStatus::BUILDING);
}

BOOST_AUTO_TEST_CASE(When_in_fail_state) {
    updater.end_failed_install();  // This sets the status to FAILED.

    BOOST_REQUIRE_EQUAL(updater.is_installed(), false);
    BOOST_REQUIRE_EQUAL(!updater.last_sent_status, false);
    BOOST_REQUIRE_EQUAL(updater.last_sent_status.get(),
                        DatastoreStatus::FAILED);

    //TODO: We shouldn't grab the real status of datastores that didn't
    //      install correctly. Of course, since the guests have no local
    //      persistant store of their own, this is difficult to gaurantee.
    //      But we can at least make sure we gaurantee it while the process
    //      runs.
    // Setting this to running simulates a false positive. It would be like if
    // the routine to see if MySQL is running returned true, even though the
    // install failed or was wonky.
    updater.next_determined_status = DatastoreStatus::RUNNING;
    void update(); // This shouldn't change anything.
    BOOST_REQUIRE_EQUAL(updater.is_installed(), false);
    BOOST_REQUIRE_EQUAL(!updater.last_sent_status, false);
    BOOST_REQUIRE_EQUAL(updater.last_sent_status.get(),
                      DatastoreStatus::FAILED);
}

BOOST_AUTO_TEST_CASE(After_datastore_is_marked_as_installed_and_pingable) {
    void update();
    BOOST_REQUIRE_EQUAL(updater.is_installed(), false);
    BOOST_REQUIRE_EQUAL(updater.last_sent_status, boost::none);

    updater.next_determined_status = DatastoreStatus::RUNNING;

    updater.end_install_or_restart();
    BOOST_REQUIRE_EQUAL(updater.is_installed(), true);
    BOOST_REQUIRE_EQUAL(!updater.last_sent_status, false);
    BOOST_REQUIRE_EQUAL(updater.last_sent_status.get(),
                        DatastoreStatus::RUNNING);
}


BOOST_AUTO_TEST_SUITE_END();

} } } // end namespace

