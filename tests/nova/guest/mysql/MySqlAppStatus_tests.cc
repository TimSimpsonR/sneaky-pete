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

using namespace nova::flags;
using nova::guest::utils::IsoDateTime;
using nova::Log;
using nova::LogOptions;
using namespace nova::db::mysql;
using nova::guest::mysql::MySqlAppStatus;
using boost::optional;
using nova::ProcessException;
using std::string;

namespace nova { namespace guest { namespace mysql {



FlagMapPtr get_flags() {
    FlagMapPtr ptr(new FlagMap());
    char * test_args = getenv("TEST_ARGS");
    BOOST_REQUIRE_MESSAGE(test_args != 0,
                          "TEST_ARGS environment var not defined.");
    if (test_args != 0) {
        ptr->add_from_arg(test_args);
    }
    return ptr;
}

struct GlobalFixture {

    LogApiScope log;
    MySqlApiScope mysql;

    GlobalFixture()
    : log(LogOptions::simple()),
      mysql()
    {
    }

    ~GlobalFixture() {
    }
};

BOOST_GLOBAL_FIXTURE(GlobalFixture);


struct MySqlAppStatusDefaultTestContext
: public virtual MySqlAppStatusContext
{
    int calls;

    MySqlAppStatusDefaultTestContext() : calls(0) {
    }

    virtual void execute(std::stringstream & out,
                         const std::list<const char *> & cmds) const {
        const_cast<MySqlAppStatusDefaultTestContext *>(this)->calls ++;
        on_execute(out, calls);
    }

    virtual void on_execute(std::stringstream & out, int call_number) const {
        BOOST_FAIL("Mock this please.");
    }

    virtual bool is_file(const char * file_path) const {
        return false;
    }

};


struct MySqlAppStatusTestsFixture {
    FlagValues flags;
    int id;
    MySqlConnectionWithDefaultDbPtr nova_db;
    MySqlAppStatus updater;

    MySqlAppStatusTestsFixture()
    :   flags(get_flags()),
        nova_db(new MySqlConnectionWithDefaultDb(flags.nova_sql_host(),
            flags.nova_sql_user(), flags.nova_sql_password(), flags.nova_sql_database())),
        updater(nova_db,
                flags.guest_ethernet_device(),
                flags.nova_sql_reconnect_wait_time(),
                optional<int>(-255),
                new MySqlAppStatusDefaultTestContext())
    {
        id = updater.get_guest_instance_id();

        CHECK_POINT();
        delete_row();
    }

    ~MySqlAppStatusTestsFixture() {
        CHECK_POINT();
        delete_row();
    }

    void delete_row() {
        MySqlPreparedStatementPtr stmt = nova_db->prepare_statement(
            "DELETE from guest_status WHERE instance_id = ?");
        stmt->set_int(0, id);
        stmt->execute(0);
    }
};


BOOST_FIXTURE_TEST_SUITE(MySqlAppStatusTestSuite,
                         MySqlAppStatusTestsFixture);

BOOST_AUTO_TEST_CASE(When_no_row_exists) {
    // Make sure no row exists in MySql db.
    CHECK_POINT();
    BOOST_CHECK_EQUAL(updater.is_mysql_installed(), false);
    BOOST_CHECK_EQUAL(updater.get_status_from_nova_db(), boost::none);
    void update(); // This shouldn't change anything.
    BOOST_CHECK_EQUAL(updater.is_mysql_installed(), false);
    BOOST_CHECK_EQUAL(updater.get_status_from_nova_db(), boost::none);
}

BOOST_AUTO_TEST_CASE(When_row_is_in_build_state) {
    CHECK_POINT();
    updater.begin_mysql_install();

    BOOST_CHECK_EQUAL(updater.is_mysql_installed(), false);
    BOOST_CHECK_EQUAL(!updater.get_status_from_nova_db(), false);
    BOOST_CHECK_EQUAL(updater.get_status_from_nova_db().get(),
                      MySqlAppStatus::BUILDING);

    CHECK_POINT();
    void update(); // This shouldn't change anything.
    BOOST_CHECK_EQUAL(updater.is_mysql_installed(), false);
    BOOST_CHECK_EQUAL(!updater.get_status_from_nova_db(), false);
    BOOST_CHECK_EQUAL(updater.get_status_from_nova_db().get(),
                      MySqlAppStatus::BUILDING);
}

BOOST_AUTO_TEST_CASE(When_row_is_in_fail_state) {
    CHECK_POINT();
    updater.set_status(MySqlAppStatus::FAILED);

    BOOST_CHECK_EQUAL(updater.is_mysql_installed(), false);
    BOOST_CHECK_EQUAL(!updater.get_status_from_nova_db(), false);
    BOOST_CHECK_EQUAL(updater.get_status_from_nova_db().get(),
                      MySqlAppStatus::FAILED);
    CHECK_POINT();
    void update(); // This shouldn't change anything.
    BOOST_CHECK_EQUAL(updater.is_mysql_installed(), false);
    BOOST_CHECK_EQUAL(!updater.get_status_from_nova_db(), false);
    BOOST_CHECK_EQUAL(updater.get_status_from_nova_db().get(),
                      MySqlAppStatus::FAILED);

}

BOOST_AUTO_TEST_CASE(After_mysql_is_marked_as_installed_and_mysql_pingable) {
    struct Mock : public virtual MySqlAppStatusDefaultTestContext {
        virtual void on_execute(std::stringstream & out, int call_number)
        const {
            // Do nothing.
        }
    };
    updater.context.reset(new Mock());

    //Even though in make-believe land we can get the status of MySQL, update
    // should still do nothing until we call "end_install_or_restart."

    void update();
    BOOST_CHECK_EQUAL(updater.is_mysql_installed(), false);
    BOOST_CHECK_EQUAL(updater.get_status_from_nova_db(), boost::none);

    updater.end_install_or_restart();
    BOOST_CHECK_EQUAL(updater.is_mysql_installed(), true);
    BOOST_CHECK_EQUAL(!updater.get_status_from_nova_db(), false);
    BOOST_CHECK_EQUAL(updater.get_status_from_nova_db().get(),
                      MySqlAppStatus::RUNNING);
}

BOOST_AUTO_TEST_CASE(After_mysql_is_marked_as_installed_and_mysql_blocked) {
    updater.begin_mysql_install(); // Just to be different.

    struct Mock : public virtual MySqlAppStatusDefaultTestContext {
        virtual void on_execute(std::stringstream & out, int call_number) const
        {
            if (call_number < 2) {
                throw ProcessException(ProcessException::EXIT_CODE_NOT_ZERO);
            }
        }
    };
    updater.context.reset(new Mock());

    //Even though in make-believe land we can get the status of MySQL, update
    // should still do nothing until we call "end_install_or_restart."

    void update();
    BOOST_CHECK_EQUAL(updater.is_mysql_installed(), false);
    BOOST_CHECK_EQUAL(!updater.get_status_from_nova_db(), false);
    BOOST_CHECK_EQUAL(updater.get_status_from_nova_db().get(),
                      MySqlAppStatus::BUILDING);

    updater.end_install_or_restart();
    BOOST_CHECK_EQUAL(updater.is_mysql_installed(), true);
    BOOST_CHECK_EQUAL(!updater.get_status_from_nova_db(), false);
    BOOST_CHECK_EQUAL(updater.get_status_from_nova_db().get(),
                      MySqlAppStatus::BLOCKED);
}

BOOST_AUTO_TEST_CASE(After_mysql_is_marked_as_installed_and_mysql_crashed) {
    // First, it calls mysqladmin - throw not zero exception.
    // Next, it tries to find a running process for MySQL. Return not zero.
    // Third, it tries to find the pid file. Return zero on this one.
    // Finally, return true for whether or not the file exists.
    // The logic should think MySQL at one time was there and assume it crashed.
    struct Mock : public virtual MySqlAppStatusDefaultTestContext {
        virtual bool is_file(const char * file_path) const {
            return true;
        }
        virtual void on_execute(std::stringstream & out, int call_number) const
        {
            if (call_number == 1 || call_number == 2) {
                throw ProcessException(ProcessException::EXIT_CODE_NOT_ZERO);
            } else {
                out << "-user=mysql --pid-file=/var/run/mysqld/mysqld.pid "
                       "--socket=/var/run/mysqld/mysqld.sock ";
            }
        }
    };
    updater.context.reset(new Mock());
    updater.end_install_or_restart();
    BOOST_CHECK_EQUAL(updater.is_mysql_installed(), true);
    BOOST_CHECK_EQUAL(!updater.get_status_from_nova_db(), false);
    BOOST_CHECK_EQUAL(updater.get_status_from_nova_db().get(),
                      MySqlAppStatus::CRASHED);
}

BOOST_AUTO_TEST_CASE(After_mysql_is_marked_as_installed_and_mysql_crashed2) {
    // First, it calls mysqladmin - throw not zero exception.
    // Next, it tries to find a running process for MySQL. Return not zero.
    // Third, it tries to find the pid file. Return zero on this one.
    // Finally, return false for whether or not the file exists.
    // Even though it got the pid, the logic should think MySQL was never there.
    struct Mock : public virtual MySqlAppStatusDefaultTestContext {
        virtual void on_execute(std::stringstream & out, int call_number) const
        {
            if (call_number == 1 || call_number == 2) {
                throw ProcessException(ProcessException::EXIT_CODE_NOT_ZERO);
            } else {
                out << "-user=mysql --pid-file=/var/run/mysqld/mysqld.pid "
                       "--socket=/var/run/mysqld/mysqld.sock ";
            }
        }
    };
    updater.context.reset(new Mock());
    updater.end_install_or_restart();
    BOOST_CHECK_EQUAL(updater.is_mysql_installed(), true);
    BOOST_CHECK_EQUAL(!updater.get_status_from_nova_db(), false);
    BOOST_CHECK_EQUAL(updater.get_status_from_nova_db().get(),
                      MySqlAppStatus::SHUTDOWN);
}

BOOST_AUTO_TEST_CASE(After_mysql_is_marked_as_installed_but_mysql_not_there) {
    struct Mock : public virtual MySqlAppStatusDefaultTestContext {
        virtual void on_execute(std::stringstream & out, int call_number) const
        {
            throw ProcessException(ProcessException::EXIT_CODE_NOT_ZERO);
        }
    };
    updater.context.reset(new Mock());
    updater.end_install_or_restart();
    BOOST_CHECK_EQUAL(updater.is_mysql_installed(), true);
    BOOST_CHECK_EQUAL(!updater.get_status_from_nova_db(), false);
    BOOST_CHECK_EQUAL(updater.get_status_from_nova_db().get(),
                      MySqlAppStatus::SHUTDOWN);
}

BOOST_AUTO_TEST_SUITE_END();

} } } // end namespace

