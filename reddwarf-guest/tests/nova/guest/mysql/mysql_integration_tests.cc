#define BOOST_TEST_MODULE Integration_Tests
#include <boost/test/unit_test.hpp>

#include "nova/guest/apt.h"
#include "nova/flags.h"
#include <boost/foreach.hpp>
#include <boost/format.hpp>
#include <boost/assign/list_of.hpp>
#include "nova/Log.h"
#include <memory>
#include "nova/guest/mysql.h"
#include "nova/guest/mysql/MySqlPreparer.h"
#include "nova/process.h"
#include <stdlib.h>
#include <unistd.h>

//#include "nova/"


namespace apt = nova::guest::apt;
using namespace boost::assign;
using std::auto_ptr;
using namespace nova::flags;
using boost::format;
using nova::Log;
using namespace nova::guest::mysql;
using nova::Process;
using sql::ResultSet;
using std::string;
using std::stringstream;

const char * ADMIN_USER_NAME = "os_admin";
const double INITIAL_USER_COUNT = 4;
const double TIME_OUT = 60;
const char * URI = "localhost"; // :5672";

Log log;

#define CHECK_POINT() BOOST_CHECK_EQUAL(2,2); log.debug("CHECKPOINT: At line # %d...", __LINE__);

#define CHECK_EXCEPTION(statement, ex_code) try { \
        statement ; \
        BOOST_FAIL("Should have thrown."); \
    } catch(const MySqlException & mse) { \
        const char * actual = MySqlException::code_to_string(MySqlException::ex_code); \
        const char * expected = MySqlException::code_to_string(mse.code); \
        BOOST_REQUIRE_EQUAL(actual, expected); \
    }

#define ASSERT_ACCESS_DENIED(func) \
    { bool passed = false; \
      try { \
        func \
        passed = true; \
      } catch(const std::exception & e) { \
        string msg(e.what());    \
        if (msg.find("Could not connect") == string::npos) {    \
            string fm = str(format("Expected 'Could not connect', error was: %s") \
                            % msg);  \
            BOOST_FAIL(fm.c_str()); \
        } \
      } \
      if (passed) { \
          BOOST_FAIL("Should have thrown an exception."); \
      } \
    }

FlagMapPtr get_flags() {
    //int argc = boost::unit_test::framework::master_test_suite().argc;
    //char ** argv = boost::unit_test::framework::master_test_suite().argv;
    //BOOST_REQUIRE_EQUAL(2, argc);
    FlagMapPtr ptr(new FlagMap());
    char * test_args = getenv("TEST_ARGS");
    BOOST_REQUIRE_MESSAGE(test_args != 0, "TEST_ARGS environment var not defined.");
    if (test_args != 0) {
        ptr->add_from_arg(test_args);
    }
    //return FlagMap::create_from_args(argc, argv, true);
    return ptr;
}

int count_query_results(MySqlConnectionPtr sql, const char * query) {
    MySqlResultSetPtr res = sql->query(query);
    while(res->next());
    return res->get_row_count();
}

int count_users(MySqlGuestPtr sql) {
    return count_query_results(sql->get_connection(), "SELECT * FROM mysql.user;");
}

MySqlConnectionPtr create_connection(const char * user, const char * pass) {
    MySqlConnectionPtr connection(new MySqlConnection(URI, user, pass));
    return connection;
}

MySqlGuestPtr create_guest(const char * user, const char * pass) {
    MySqlGuestPtr guest(new MySqlGuest(URI, user, pass));
    return guest;
}

MySqlGuestPtr create_root_guest() {
    return create_guest("root", "");
}

bool db_list_contains(MySqlDatabaseListPtr databases, const char * dbname) {
    bool found = false;
    BOOST_FOREACH(MySqlDatabasePtr db, *databases) {
        if (db->get_name() == dbname) {
            found = true;
        }
    }
    return found;
}

int execute_query(MySqlConnectionPtr sql, const char * query) {
    MySqlResultSetPtr res = sql->query(query);
    while(res->next());
    return res->get_row_count();
}

bool is_mysql_running() {
    CHECK_POINT();
    Process::CommandList cmds = list_of("/usr/sbin/service")("--status-all");
    stringstream outstream;
    CHECK_POINT();
    Process::execute(outstream, cmds, 30);
    CHECK_POINT();
    return (outstream.str().find("mysql") != string::npos);
}

int number_of_admins(MySqlGuestPtr sql) {
    CHECK_POINT();
    return count_query_results(sql->get_connection(),
        "SELECT * FROM mysql.user \n"
        "     WHERE User='os_admin'\n"
        "     AND Host='localhost'");
}

bool user_list_contains(MySqlUserListPtr users, const char * user_name) {
    bool found = false;
    BOOST_FOREACH(MySqlUserPtr user, *users) {
        if (user->get_name() == user_name) {
            found = true;
        }
    }
    return found;
}



void set_up_tests() {
    { // Wipe
        apt::remove("mysql-server-5.1", TIME_OUT);
        //apt::remove("mysql-server", TIME_OUT);
        // BOOST_REQUIRE( ! is_mysql_running());
        // Make sure the directories are gone too so the old usernames
        // and tables do not resurrect.
        CHECK_POINT();
        Process rm_proc(list_of("/usr/bin/sudo")("rm")("-rf")("/var/lib/mysql"),
                        true);
        stringstream str;
        CHECK_POINT();
        rm_proc.wait_for_eof(str, TIME_OUT);
        CHECK_POINT();
        Process ls_proc(list_of("/bin/ls")("/var/lib/mysql"), true);
        stringstream ls_out;
        CHECK_POINT();
        ls_proc.wait_for_eof(ls_out, TIME_OUT);
        bool found = ls_out.str().find("cannot access /var/lib/mysql: No such "
                                       "file or directory", 0) == string::npos;
        CHECK_POINT();
        BOOST_REQUIRE_MESSAGE(!found, "mysql directory not deleted.");

    }
    { // Install MySQL
        // We don't need a real connection to call some methods, as
        // demonstrated by the following bit of nonsense:
        MySqlGuestPtr guest = create_guest("gdfjkgh", "gdfhfsdah");
        CHECK_POINT();
        MySqlPreparerPtr prepare(new MySqlPreparer(guest));
        CHECK_POINT();
        prepare->install_mysql();
        CHECK_POINT();
        sleep(3);
        CHECK_POINT();
        BOOST_REQUIRE(is_mysql_running());
        CHECK_POINT();
    }
}

void mysql_guest_tests(MySqlGuestPtr guest, const int initial_user_count) {
    CHECK_POINT();
    MySqlDatabasePtr db_test1(new MySqlDatabase());
    CHECK_POINT();
    db_test1->set_name("test1");
    db_test1->set_character_set("utf8");
    db_test1->set_collation("utf8_general_ci");
    MySqlDatabasePtr db_test2(new MySqlDatabase());
    db_test2->set_name("test2");
    db_test2->set_character_set("utf8");
    db_test2->set_collation("utf8_general_ci");
    CHECK_POINT();

    { // initial list of databases is empty
        MySqlDatabaseListPtr db_list = guest->list_databases();
        CHECK_POINT();
        BOOST_REQUIRE_EQUAL(db_list->size(), 0);
        BOOST_REQUIRE( ! db_list_contains(db_list, "test1"));
        BOOST_REQUIRE( ! db_list_contains(db_list, "test2"));
    }
    {
        MySqlDatabaseListPtr db_list(new MySqlDatabaseList());
        db_list->push_back(db_test1);
        db_list->push_back(db_test2);
        CHECK_POINT();
        guest->create_database(db_list);
    }
    {
        CHECK_POINT();
        MySqlDatabaseListPtr db_list = guest->list_databases();
        BOOST_REQUIRE_EQUAL(db_list->size(), 2);
        BOOST_REQUIRE(db_list_contains(db_list, "test1"));
        BOOST_REQUIRE(db_list_contains(db_list, "test2"));
    }

    // Now, create a user.
    MySqlUserPtr john_doe(new MySqlUser());
    john_doe->set_name("JohnDoe");
    john_doe->set_password("password");
    john_doe->get_databases()->push_back(db_test1);
    { // First, list users and ensure we see nothing.
        BOOST_REQUIRE_EQUAL(initial_user_count, count_users(guest));
        CHECK_POINT();
        MySqlUserListPtr user_list = guest->list_users();
        CHECK_POINT();
        //Note: list_users() doesn't include some users.
        BOOST_REQUIRE_EQUAL(0, user_list->size());
        CHECK_POINT();
        BOOST_REQUIRE( ! user_list_contains(user_list, "JohnDoe"));
        CHECK_POINT();
    }
    { // Create user.
        MySqlUserListPtr list(new MySqlUserList());
        list->push_back(john_doe);
        CHECK_POINT();
        guest->create_users(list);
    }
    {
        CHECK_POINT();
        // List users (should see JohnDoe)
        BOOST_REQUIRE_EQUAL(initial_user_count + 1, count_users(guest));
        MySqlUserListPtr user_list = guest->list_users();
        //Note: list_users() doesn't include some users.
        BOOST_REQUIRE_EQUAL(1, user_list->size());
        CHECK_POINT();
        BOOST_REQUIRE(user_list_contains(user_list, "JohnDoe"));
        CHECK_POINT();
        guest->list_users();
        CHECK_POINT();
        MySqlConnectionPtr doe = create_connection("JohnDoe", "password");
        CHECK_POINT();
        execute_query(doe, "use test1");
        // Make sure JohnDoe can access his database
        execute_query(doe,
            "CREATE TABLE test1.table1 (name CHAR(50))");
        // Make sure JohnDoe cannot access someone else's database.
        CHECK_POINT();
        CHECK_EXCEPTION({ execute_query(doe, "use test2"); }, QUERY_FAILED);
        CHECK_EXCEPTION({
            execute_query(doe, "CREATE TABLE test2.table2(name CHAR(50))");
        }, QUERY_FAILED);
        CHECK_POINT();
    }

    { // Change JohnDoe's password.
        guest->set_password("JohnDoe", "ReMiFaSo");
        // Logging in as the old password should fail...
        MySqlConnectionPtr old_doe = create_connection("JohnDoe", "password");
        ASSERT_ACCESS_DENIED({
            old_doe->init();
        });
        // Should work with new one.
        MySqlConnectionPtr new_doe = create_connection("JohnDoe", "ReMiFaSo");
        new_doe->init();
    }

    { // Time to say good-bye to Mr. John Doe.
        guest->delete_user("JohnDoe");
        sleep(1);
        BOOST_REQUIRE_EQUAL(initial_user_count, count_users(guest));
        MySqlUserListPtr user_list = guest->list_users();
        //Note: list_users() doesn't include some users.
        BOOST_REQUIRE_EQUAL(0, user_list->size());
        BOOST_REQUIRE( ! user_list_contains(user_list, "JohnDoe"));
        // This next part is overkill but oh well.
        MySqlConnectionPtr old_doe = create_connection("JohnDoe", "ReMiFaSo");
        ASSERT_ACCESS_DENIED({
            old_doe->init();
        });
    }

    {   // Initially, root should not be enabled.
        BOOST_REQUIRE( ! guest->is_root_enabled());
        MySqlUserPtr root_user = guest->enable_root();

        // Root user should work.
        BOOST_REQUIRE_EQUAL(root_user->get_name(), "root");
        MySqlConnectionPtr rudy = create_connection(
            "root", root_user->get_password().c_str());
        rudy->init();

        // It should also now tell us it is in fact enabled.
        BOOST_REQUIRE(guest->is_root_enabled());
    }

    { // Time to destroy our databases.
        guest->delete_database("test1");
        MySqlDatabaseListPtr db_list = guest->list_databases();
        BOOST_REQUIRE_EQUAL(db_list->size(), 1);
        BOOST_REQUIRE( ! db_list_contains(db_list, "test1"));
        BOOST_REQUIRE(db_list_contains(db_list, "test2"));
    }
    {
        guest->delete_database("test2");
        MySqlDatabaseListPtr db_list = guest->list_databases();
        BOOST_REQUIRE_EQUAL(db_list->size(), 0);
        BOOST_REQUIRE( ! db_list_contains(db_list, "test1"));
        BOOST_REQUIRE( ! db_list_contains(db_list, "test2"));
    }

}

std::string preparation_tests() {
    string admin_password;

    CHECK_POINT();
    MySqlGuestPtr root_client = create_root_guest();
    CHECK_POINT();
    MySqlPreparerPtr prepare(new MySqlPreparer(root_client));

    { // Add Admin user
        CHECK_POINT();
        BOOST_REQUIRE_EQUAL(number_of_admins(root_client), 0);
        CHECK_POINT();
        admin_password = generate_password();
        CHECK_POINT();
        prepare->create_admin_user(admin_password);
        CHECK_POINT();
        BOOST_REQUIRE_EQUAL(number_of_admins(root_client), 1);
        CHECK_POINT();
    }

    CHECK_POINT();
    MySqlGuestPtr admin;
    { // Log in as admin...
        admin.reset(new MySqlGuest("localhost", ADMIN_USER_NAME,
                                   admin_password.c_str()));
        BOOST_REQUIRE_EQUAL(number_of_admins(admin), 1);
    }

    { // Generate root password, making it so *we* cannot log in.
        CHECK_POINT();
        prepare->generate_root_password();
        CHECK_POINT();
        MySqlGuestPtr new_root_client = create_root_guest();
        CHECK_POINT();
        ASSERT_ACCESS_DENIED({number_of_admins(new_root_client);});
        CHECK_POINT();
    }

    CHECK_POINT();
    { // Create an anonymous user...
        admin->get_connection()->query("CREATE USER '';");
        int count = count_query_results(admin->get_connection(),
            "SELECT * FROM mysql.user WHERE User='';");
        BOOST_REQUIRE_EQUAL(count, 1);
    }

    CHECK_POINT();
    { // ... remove the anonymous user (test preparer function)
        prepare->remove_anonymous_user();
        int count = count_query_results(admin->get_connection(),
            "SELECT * FROM mysql.user WHERE User='';");
        BOOST_REQUIRE_EQUAL(count, 0);
    }

    CHECK_POINT();
    { // Create remote root user...
        admin->get_connection()->query(
            "CREATE USER 'root'@'123.123.123.123' IDENTIFIED BY 'password';");
        int count = count_query_results(admin->get_connection(),
            "SELECT * FROM mysql.user WHERE User='root' "
                                     "AND Host='123.123.123.123'");
        BOOST_REQUIRE_EQUAL(count, 1);
    }
    CHECK_POINT();
    { // ... remove root user
        prepare->remove_remote_root_access();
        int count = count_query_results(admin->get_connection(),
            "SELECT * FROM mysql.user WHERE User='root' "
                                     "AND Host='123.123.123.123'");
        BOOST_REQUIRE_EQUAL(count, 0);
    }


#ifdef INTEGRATION_WITH_REDDWARF_CI_COMPLETE
    { // Init mycnf
        //TODO: Reenable once packages are here
        //prepare->init_mycnf(admin_password);
        Process sql(list_of("/usr/bin/mysql"), false);
        {
            stringstream out;
            sql.read_until_pause(out, 1, 30);
            BOOST_REQUIRE(out.str().find("mysql>", 0) != string::npos);
        }
        {
            stringstream out;
            sql.write("CREATE USER test@'localhost';");
            sql.read_until_pause(out, 2, 30);
            BOOST_REQUIRE(out.str().find("Query OK, 0 rows affected", 0)
                          != string::npos);
        }
        {
            stringstream out;
            sql.write("GRANT ALL PRIVILEGES ON *.* TO test@'localhost' "
                      "WITH GRANT OPTION;");
            sql.read_until_pause(out, 2, 30);
            BOOST_REQUIRE(out.str().find("Query OK, 0 rows affected", 0)
                          != string::npos);
        }
        sql.write("exit");
    }

    { // restart mysql
        // original_size = os.stat("/var/lib/mysql/ib_logfile0").st_size
        // self.prepare.restart_mysql()
        // new_size = os.stat("/var/lib/mysql/ib_logfile0").st_size
        // if original_size >= new_size:
        //     fail("The size of the logfile has not increased. "
        //          "Old size=" + str(original_size) + ", "
        //          "new size=" + str(new_size))
        // assert_true(original_size < new_size)
        // assert_true(15000000L < new_size);
    }

    { // A fully set up system should return false for is_root_enabled.
        BOOST_REQURIE( ! root_client->is_root_enabled());
    }
#endif

    return admin_password;
}

BOOST_AUTO_TEST_CASE(integration_tests)
{
    const char * value = getenv("DESTROY_MYSQL_ON_THIS_MACHINE");
    if (value == NULL || string(value) != "PLEASE") {
        BOOST_FAIL("Cannot run the integration tests unless the user is "
                   "ok with their MySQL install being destroyed.");
    } else {
        CHECK_POINT();
        set_up_tests();
        CHECK_POINT();

        CHECK_POINT();
        string admin_password = preparation_tests();
        CHECK_POINT();

        #ifdef INTEGRATION_WITH_REDDWARF_CI_COMPLETE
            // Using the admin_password returned above is a cheat-
            // the mycnf file should know the correct values for the admin user.
            MySqlGuestPtr admin_guest(new MySqlGuest(URI));
        #else
            MySqlGuestPtr admin_guest = create_guest(ADMIN_USER_NAME,
                                                     admin_password.c_str());
        #endif
        CHECK_POINT();
        mysql_guest_tests(admin_guest, 3);
        CHECK_POINT();
    }
}
