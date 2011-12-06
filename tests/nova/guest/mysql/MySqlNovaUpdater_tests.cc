#define BOOST_TEST_MODULE nova_guest_mysql_MySqlNovaUpdater_tests
#include <boost/test/unit_test.hpp>

// Gentlemen, prepare for maximum LOLs!
#define private public
#define protected public

#include "nova/flags.h"
#include <boost/optional.hpp>
#include "nova/Log.h"
#include "nova/db/mysql.h"
#include "nova/guest/mysql/MySqlNovaUpdater.h"
#include "nova/process.h"
#include "nova/guest/utils.h"

#define CHECK_POINT() BOOST_CHECK_EQUAL(2,2);

using namespace nova::flags;
using nova::guest::utils::IsoDateTime;
using nova::Log;
using namespace nova::db::mysql;
using nova::guest::mysql::MySqlNovaUpdater;
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
    GlobalFixture() {
        MySqlConnection::start_up();
    }

    ~GlobalFixture() {
        MySqlConnection::shut_down();
    }
};

BOOST_GLOBAL_FIXTURE(GlobalFixture);


struct MySqlNovaUpdaterDefaultTestContext
: public virtual MySqlNovaUpdaterContext
{
    int calls;

    MySqlNovaUpdaterDefaultTestContext() : calls(0) {
    }

    virtual void execute(std::stringstream & out,
                         const std::list<const char *> & cmds) const {
        const_cast<MySqlNovaUpdaterDefaultTestContext *>(this)->calls ++;
        on_execute(out, calls);
    }

    virtual void on_execute(std::stringstream & out, int call_number) const {
        BOOST_FAIL("Mock this please.");
    }

    virtual bool is_file(const char * file_path) const {
        return false;
    }

};


struct MySqlNovaUpdaterTestsFixture {
    FlagValues flags;
    int id;
    MySqlConnectionPtr nova_db;
    MySqlNovaUpdater updater;

    MySqlNovaUpdaterTestsFixture()
    :   flags(get_flags()),
        nova_db(new MySqlConnection(flags.nova_sql_host(),
            flags.nova_sql_user(), flags.nova_sql_password())),
        updater(nova_db, flags.nova_sql_database(),
                flags.guest_ethernet_device(),
                flags.nova_sql_reconnect_wait_time(),
                optional<int>(-255),
                new MySqlNovaUpdaterDefaultTestContext())
    {
        id = updater.get_guest_instance_id();

        CHECK_POINT();
        delete_row();
    }

    ~MySqlNovaUpdaterTestsFixture() {
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


BOOST_FIXTURE_TEST_SUITE(MySqlNovaUpdaterTestSuite,
                         MySqlNovaUpdaterTestsFixture);

BOOST_AUTO_TEST_CASE(When_no_row_exists) {
    // Make sure no row exists in MySql db.
    CHECK_POINT();
    BOOST_CHECK_EQUAL(updater.mysql_is_installed(), false);
    BOOST_CHECK_EQUAL(updater.get_status_from_nova_db(), boost::none);
    void update(); // This shouldn't change anything.
    BOOST_CHECK_EQUAL(updater.mysql_is_installed(), false);
    BOOST_CHECK_EQUAL(updater.get_status_from_nova_db(), boost::none);
}

BOOST_AUTO_TEST_CASE(When_row_is_in_build_state) {
    CHECK_POINT();
    updater.begin_mysql_install();

    BOOST_CHECK_EQUAL(updater.mysql_is_installed(), false);
    BOOST_CHECK_EQUAL(!updater.get_status_from_nova_db(), false);
    BOOST_CHECK_EQUAL(updater.get_status_from_nova_db().get(),
                      MySqlNovaUpdater::BUILDING);

    CHECK_POINT();
    void update(); // This shouldn't change anything.
    BOOST_CHECK_EQUAL(updater.mysql_is_installed(), false);
    BOOST_CHECK_EQUAL(!updater.get_status_from_nova_db(), false);
    BOOST_CHECK_EQUAL(updater.get_status_from_nova_db().get(),
                      MySqlNovaUpdater::BUILDING);
}

BOOST_AUTO_TEST_CASE(When_row_is_in_fail_state) {
    CHECK_POINT();
    updater.set_status(MySqlNovaUpdater::FAILED);

    BOOST_CHECK_EQUAL(updater.mysql_is_installed(), false);
    BOOST_CHECK_EQUAL(!updater.get_status_from_nova_db(), false);
    BOOST_CHECK_EQUAL(updater.get_status_from_nova_db().get(),
                      MySqlNovaUpdater::FAILED);
    CHECK_POINT();
    void update(); // This shouldn't change anything.
    BOOST_CHECK_EQUAL(updater.mysql_is_installed(), false);
    BOOST_CHECK_EQUAL(!updater.get_status_from_nova_db(), false);
    BOOST_CHECK_EQUAL(updater.get_status_from_nova_db().get(),
                      MySqlNovaUpdater::FAILED);

}

BOOST_AUTO_TEST_CASE(After_mysql_is_marked_as_installed_and_mysql_pingable) {
    struct Mock : public virtual MySqlNovaUpdaterDefaultTestContext {
        virtual void on_execute(std::stringstream & out, int call_number)
        const {
            // Do nothing.
        }
    };
    updater.context.reset(new Mock());

    //Even though in make-believe land we can get the status of MySQL, update
    // should still do nothing until we call "mark_mysql_as_installed."

    void update();
    BOOST_CHECK_EQUAL(updater.mysql_is_installed(), false);
    BOOST_CHECK_EQUAL(updater.get_status_from_nova_db(), boost::none);

    updater.mark_mysql_as_installed();
    BOOST_CHECK_EQUAL(updater.mysql_is_installed(), true);
    BOOST_CHECK_EQUAL(!updater.get_status_from_nova_db(), false);
    BOOST_CHECK_EQUAL(updater.get_status_from_nova_db().get(),
                      MySqlNovaUpdater::RUNNING);
}

BOOST_AUTO_TEST_CASE(After_mysql_is_marked_as_installed_and_mysql_blocked) {
    updater.begin_mysql_install(); // Just to be different.

    struct Mock : public virtual MySqlNovaUpdaterDefaultTestContext {
        virtual void on_execute(std::stringstream & out, int call_number) const
        {
            if (call_number < 2) {
                throw ProcessException(ProcessException::EXIT_CODE_NOT_ZERO);
            }
        }
    };
    updater.context.reset(new Mock());

    //Even though in make-believe land we can get the status of MySQL, update
    // should still do nothing until we call "mark_mysql_as_installed."

    void update();
    BOOST_CHECK_EQUAL(updater.mysql_is_installed(), false);
    BOOST_CHECK_EQUAL(!updater.get_status_from_nova_db(), false);
    BOOST_CHECK_EQUAL(updater.get_status_from_nova_db().get(),
                      MySqlNovaUpdater::BUILDING);

    updater.mark_mysql_as_installed();
    BOOST_CHECK_EQUAL(updater.mysql_is_installed(), true);
    BOOST_CHECK_EQUAL(!updater.get_status_from_nova_db(), false);
    BOOST_CHECK_EQUAL(updater.get_status_from_nova_db().get(),
                      MySqlNovaUpdater::BLOCKED);
}

BOOST_AUTO_TEST_CASE(After_mysql_is_marked_as_installed_and_mysql_crashed) {
    // First, it calls mysqladmin - throw not zero exception.
    // Next, it tries to find a running process for MySQL. Return not zero.
    // Third, it tries to find the pid file. Return zero on this one.
    // Finally, return true for whether or not the file exists.
    // The logic should think MySQL at one time was there and assume it crashed.
    struct Mock : public virtual MySqlNovaUpdaterDefaultTestContext {
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
    updater.mark_mysql_as_installed();
    BOOST_CHECK_EQUAL(updater.mysql_is_installed(), true);
    BOOST_CHECK_EQUAL(!updater.get_status_from_nova_db(), false);
    BOOST_CHECK_EQUAL(updater.get_status_from_nova_db().get(),
                      MySqlNovaUpdater::CRASHED);
}

BOOST_AUTO_TEST_CASE(After_mysql_is_marked_as_installed_and_mysql_crashed2) {
    // First, it calls mysqladmin - throw not zero exception.
    // Next, it tries to find a running process for MySQL. Return not zero.
    // Third, it tries to find the pid file. Return zero on this one.
    // Finally, return false for whether or not the file exists.
    // Even though it got the pid, the logic should think MySQL was never there.
    struct Mock : public virtual MySqlNovaUpdaterDefaultTestContext {
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
    updater.mark_mysql_as_installed();
    BOOST_CHECK_EQUAL(updater.mysql_is_installed(), true);
    BOOST_CHECK_EQUAL(!updater.get_status_from_nova_db(), false);
    BOOST_CHECK_EQUAL(updater.get_status_from_nova_db().get(),
                      MySqlNovaUpdater::SHUTDOWN);
}

BOOST_AUTO_TEST_CASE(After_mysql_is_marked_as_installed_but_mysql_not_there) {
    struct Mock : public virtual MySqlNovaUpdaterDefaultTestContext {
        virtual void on_execute(std::stringstream & out, int call_number) const
        {
            throw ProcessException(ProcessException::EXIT_CODE_NOT_ZERO);
        }
    };
    updater.context.reset(new Mock());
    updater.mark_mysql_as_installed();
    BOOST_CHECK_EQUAL(updater.mysql_is_installed(), true);
    BOOST_CHECK_EQUAL(!updater.get_status_from_nova_db(), false);
    BOOST_CHECK_EQUAL(updater.get_status_from_nova_db().get(),
                      MySqlNovaUpdater::SHUTDOWN);
}

BOOST_AUTO_TEST_SUITE_END();

} } } // end namespace

