#define BOOST_TEST_MODULE nova_guest_mysql_MySqlAppStatus_tests
#include <boost/test/unit_test.hpp>

// Gentlemen, prepare for maximum LOLs!
#define private public
#define protected public

#include "nova/flags.h"
#include <boost/optional.hpp>
#include "nova/Log.h"
#include "nova/db/mysql.h"
#include "nova/guest/mysql/MySqlAppStatus.h"
#include "nova/process.h"
#include "nova/guest/utils.h"

#define CHECK_POINT() BOOST_CHECK_EQUAL(2,2);

using namespace nova::rpc;
using namespace nova::datastores;
using namespace nova::flags;
using nova::guest::utils::IsoDateTime;
using nova::Log;
using nova::LogOptions;
using namespace nova::db::mysql;
using nova::guest::mysql::MySqlAppStatus;
using boost::optional;
using nova::process::ProcessException;
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

class TestMySqlStatus : public MySqlAppStatus {
    public:
        TestMySqlStatus(bool is_installed)
        : MySqlAppStatus(ResilientSenderPtr(), is_installed),
          call_number(0),
          last_sent_status() {
        }

        virtual void execute(std::stringstream & out,
                             const std::list<std::string> & cmds) const {
            TestMySqlStatus * mutable_this = const_cast<TestMySqlStatus *>(this);
            mutable_this->call_number += 1;
            mutable_this->on_execute();
        }

        virtual void on_execute() = 0;

        int call_number;

        optional<Status> last_sent_status;

    protected:
        virtual void set_status(Status status) {
            last_sent_status = status;
        }
};


BOOST_AUTO_TEST_CASE(After_mysql_is_marked_as_installed_and_mysql_pingable) {
    // This class mocks MySQL being pingable and working, by making
    // "execute" not throw anything. So this simulates what happens in
    // the code when MySQL can be pinged.
    struct Updater : public TestMySqlStatus {
        Updater()
        :   TestMySqlStatus(false)
        {
        }

        virtual void on_execute() {
            // Do nothing.
        }
    } updater;

    //Even though in make-believe land we can get the status of MySQL, update
    // should still do nothing until we call "end_install_or_restart."

    updater.update();
    BOOST_CHECK_EQUAL(!updater.last_sent_status, true);
    BOOST_CHECK_EQUAL(updater.last_sent_status, boost::none);

    updater.end_install_or_restart();
    BOOST_CHECK_EQUAL(updater.is_installed(), true);
    BOOST_CHECK_EQUAL(!updater.last_sent_status, false);
    BOOST_CHECK_EQUAL(updater.last_sent_status.get(), DatastoreStatus::RUNNING);
}

BOOST_AUTO_TEST_CASE(After_mysql_is_marked_as_installed_and_mysql_blocked) {
    // This class mocks MySQL being pingable and working for the first call,
    // but the second time throws an exception. This is simulates what happens
    // when MySQL is "blocked" - the first ping fails, but the second
    // process calls work.
    struct Updater : public TestMySqlStatus {
        Updater()
        :   TestMySqlStatus(false)
        {
        }

        virtual void on_execute() {
            if (call_number < 2) {
                throw ProcessException(ProcessException::EXIT_CODE_NOT_ZERO);
            }
        }
    } updater;


    updater.begin_install();

    //Even though in make-believe land we can get the status of MySQL, update
    // should still do nothing until we call "end_install_or_restart."

    void update();
    BOOST_CHECK_EQUAL(updater.is_installed(), false);
    BOOST_CHECK_EQUAL(!updater.last_sent_status, false);
    BOOST_CHECK_EQUAL(updater.last_sent_status.get(), MySqlAppStatus::BUILDING);

    updater.end_install_or_restart();
    BOOST_CHECK_EQUAL(updater.is_installed(), true);
    BOOST_CHECK_EQUAL(!updater.last_sent_status, false);
    BOOST_CHECK_EQUAL(updater.last_sent_status.get(), MySqlAppStatus::BLOCKED);
}

BOOST_AUTO_TEST_CASE(After_mysql_is_marked_as_installed_and_mysql_crashed) {
    // First, it calls mysqladmin - throw not zero exception.
    // Next, it tries to find a running process for MySQL. Return not zero.
    // Third, it tries to find the pid file. Return zero on this one.
    // Finally, return true for whether or not the file exists.
    // The logic should think MySQL at one time was there and assume it crashed.
    struct Updater : public TestMySqlStatus {
        Updater()
        :   TestMySqlStatus(false)
        {
        }

        virtual bool is_file(const char * file_path) const {
            return true;
        }

        virtual void execute(std::stringstream & out,
                             const std::list<std::string> & cmds) const {
            Updater * mutable_this = const_cast<Updater *>(this);
            mutable_this->call_number += 1;
            if (call_number == 1 || call_number == 2) {
                throw ProcessException(ProcessException::EXIT_CODE_NOT_ZERO);
            } else {
                out << "-user=mysql --pid-file=/var/run/mysqld/mysqld.pid "
                       "--socket=/var/run/mysqld/mysqld.sock ";
            }
        }

        virtual void on_execute() {
        }

    } updater;

    updater.end_install_or_restart();
    BOOST_CHECK_EQUAL(updater.is_installed(), true);
    BOOST_CHECK_EQUAL(!updater.last_sent_status, false);
    BOOST_CHECK_EQUAL(updater.last_sent_status.get(),
                      MySqlAppStatus::CRASHED);
}

BOOST_AUTO_TEST_CASE(After_mysql_is_marked_as_installed_and_mysql_crashed2) {
    // First, it calls mysqladmin - throw not zero exception.
    // Next, it tries to find a running process for MySQL. Return not zero.
    // Third, it tries to find the pid file. Return zero on this one.
    // Finally, return false for whether or not the file exists.
    // Even though it got the pid, the logic should think MySQL was never there.
    struct Updater : public TestMySqlStatus {
        Updater()
        :   TestMySqlStatus(false)
        {
        }

        virtual bool is_file(const char * file_path) const {
            return false;
        }

        virtual void execute(std::stringstream & out,
                             const std::list<std::string> & cmds) const {
            Updater * mutable_this = const_cast<Updater *>(this);
            mutable_this->call_number += 1;
            if (call_number == 1 || call_number == 2) {
                throw ProcessException(ProcessException::EXIT_CODE_NOT_ZERO);
            } else {
                out << "-user=mysql --pid-file=/var/run/mysqld/mysqld.pid "
                       "--socket=/var/run/mysqld/mysqld.sock ";
            }
        }

        virtual void on_execute() {
        }

    } updater;

    updater.end_install_or_restart();
    BOOST_CHECK_EQUAL(updater.is_installed(), true);
    BOOST_CHECK_EQUAL(!updater.last_sent_status, false);
    BOOST_CHECK_EQUAL(updater.last_sent_status.get(),
                      DatastoreStatus::SHUTDOWN);
}

BOOST_AUTO_TEST_CASE(After_mysql_is_marked_as_installed_but_mysql_not_there) {
    struct Updater : public TestMySqlStatus {
        Updater()
        :   TestMySqlStatus(false)
        {
        }

        virtual void on_execute() {
            throw ProcessException(ProcessException::EXIT_CODE_NOT_ZERO);
        }
    } updater;

    updater.end_install_or_restart();
    BOOST_CHECK_EQUAL(updater.is_installed(), true);
    BOOST_CHECK_EQUAL(!updater.last_sent_status, false);
    BOOST_CHECK_EQUAL(updater.last_sent_status.get(),
                      DatastoreStatus::SHUTDOWN);
}


} } } // end namespace

