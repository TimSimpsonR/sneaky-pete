import os
import time
import unittest

import pexpect
from proboscis import before_class
from proboscis import test
from proboscis.asserts import assert_equal
from proboscis.asserts import assert_false
from proboscis.asserts import assert_true
from proboscis.asserts import assert_is_not_none
from proboscis.asserts import fail
from sqlalchemy import create_engine
from sqlalchemy.sql.expression import text

#from sqlalchemy.sql.expression import text

import nova_guest_apt as apt
from nova_guest_mysql import MySqlDatabase
from nova_guest_mysql import MySqlGuest
from nova_guest_mysql import MySqlPreparer
from nova_guest_mysql import MySqlUser



# Change this to False to run the tests in an environment that can handle them.
ADMIN_USER_NAME = "os_admin"
FLUSH = text("""FLUSH PRIVILEGES;""")
TIME_OUT = 60

admin_password = None


def assert_raises_access_denied(func, *args):
    passes = False
    try:
        func(*args)
        passes = True
    except Exception as ex:
        if not str(ex).find("Access denied") >= 0:
            fail("Expected 'Access denied' in exception! Got:"+ str(ex))
    if passes:
        fail("Method ran without throwing an exception. :(")



class LocalSqlClient(object):
    """A sqlalchemy wrapper to manage transactions"""

    def __init__(self, engine):
        self.engine = engine

    def __enter__(self):
        print("New connection...")
        self.conn = self.engine.connect()
        self.trans = self.conn.begin()
        return self.conn

    def __exit__(self, type, value, traceback):
        if self.trans:
            if type is not None:  # An error occurred
                print("Rolling back...")
                self.trans.rollback()
            else:
                print("Committing...")
                self.conn.execute(FLUSH)
                self.trans.commit()
        print("Closing connection.")
        self.conn.close()

    def execute(self, t, **kwargs):
        try:
            return self.conn.execute(t, kwargs)
        except:
            self.trans.rollback()
            self.trans = None
            raise


@test
class PasswordGeneration(unittest.TestCase):
    """Some ridiculous tests for the password generation."""

    def test_must_be_str(self):
        password = MySqlGuest.generate_password()
        self.assertTrue(isinstance(password, str))

    def test_must_be_unique(self):
        last_password = None
        for i in range(10):
            password = MySqlGuest.generate_password()
            self.assertNotEqual(password, last_password)
            last_password = password

    def test_must_be_kind_of_long(self):
        password = MySqlGuest.generate_password()
        self.assertTrue(len(password) > 10)


class MySqlTest(object):

    def __init__(self):
        self.guest = MySqlGuest("localhost:5672", "root", "")
        self.prepare = MySqlPreparer(self.guest)


@test
class MySqlInit(MySqlTest):
    """Initializes MySQL to a clean slate."""

    @staticmethod
    def is_mysql_running():
        try:
            proc = pexpect.spawn("service --status-all")
            proc.expect("mysql")
            return True
        except pexpect.ExceptionPexpect:
            return False

    @before_class
    def wipe(self):
        apt.remove("mysql-server-5.1", TIME_OUT);
        assert_false(self.is_mysql_running())
        # Take no chances. This, too, must die.
        proc = pexpect.spawn("sudo rm -rf /var/lib/mysql")
        proc.expect(pexpect.EOF)
        proc.close()

    @test
    def install_mysql(self):
        #self.guest.flush_privileges()
        self.prepare.install_mysql()
        time.sleep(3)
        assert_true(self.is_mysql_running())


@test(groups=["general"])
class MySqlGuestTests(MySqlTest):
    """Tests various MySql guest functions."""

    @test
    def create_db(self):
        db = MySqlDatabase()
        db.name = "test"
        self.guest.create_database([db])



@test(depends_on=[MySqlInit])
class MySqlPreparerTests(object):
    """Tests the MySQL installation process."""

    def __init__(self):
        self.admin_password = None  # This is set later.
        self.admin_sql = None  # This is set later
        self.guest = MySqlGuest("localhost:5672", "root", "")
        self.prepare = MySqlPreparer(self.guest)

    @staticmethod
    def root_engine():
        return create_engine("mysql://root:@localhost:5672", echo=True)

    @staticmethod
    def number_of_admins(sql):
        with sql:
            rs = sql.execute("""
                SELECT * FROM mysql.user
                           WHERE User='%s'
                           AND Host='localhost'"""
                    % ADMIN_USER_NAME)
            return len(rs.fetchall())

    @test()
    def add_admin_user(self):
        root_sql = LocalSqlClient(self.root_engine())
        assert_equal(self.number_of_admins(root_sql), 0)
        self.admin_password = self.guest.generate_password()
        with root_sql:
            self.prepare.create_admin_user(self.admin_password)
        assert_equal(self.number_of_admins(root_sql), 1)

    @test(depends_on=[add_admin_user])
    def should_be_able_to_log_in_as_admin(self):
        # Now connect via the new admin user.
        engine = create_engine("mysql://%s:%s@localhost:5672"
                               % (ADMIN_USER_NAME, self.admin_password),
                               echo=True)
        self.admin_sql = LocalSqlClient(engine)
        assert_equal(self.number_of_admins(self.admin_sql), 1)

    @test(depends_on=[should_be_able_to_log_in_as_admin])
    def generate_root_password(self):
        """
        Generate a random root password, making it impossible for someone
        to log in as root.
        """
        self.prepare.generate_root_password();

        def connect_with_root():
            root_sql = LocalSqlClient(self.root_engine())
            self.number_of_admins(root_sql)

        assert_raises_access_denied(connect_with_root)


    @test(depends_on=[should_be_able_to_log_in_as_admin])
    def remove_anon_user(self):
        """Remove anonymous user."""
        sql = self.admin_sql
        with sql:
            try:
                sql.execute("""CREATE USER '';""")
            except Exception:
                pass

        with sql:
            rs = sql.execute("""SELECT * FROM mysql.user WHERE User='';""")
            assert_equal(1, len(rs.fetchall()))

        with sql:
            self.prepare.remove_anonymous_user()

        with sql:
            rs = sql.execute("""SELECT * FROM mysql.user WHERE User='';""")
            assert_equal(0, len(rs.fetchall()))

    @test(depends_on=[should_be_able_to_log_in_as_admin])
    def remove_remote_root_access(self):
        """Create a remote root user, and then remove it."""
        sql = self.admin_sql
        with sql:
            try:
                sql.execute("""
                    CREATE USER 'root'@'123.123.123.123'
                        IDENTIFIED BY 'password';
                    """)
            except Exception:
                pass

        with sql:
            rs = sql.execute("""
                SELECT * FROM mysql.user
                    WHERE User='root'
                    AND Host='123.123.123.123';
                """)
            assert_equal(1, len(rs.fetchall()))

        with sql:
            self.prepare.remove_remote_root_access()

        with sql:
            rs = sql.execute("""
                SELECT * FROM mysql.user
                    WHERE User='root'
                    AND Host != 'localhost';
                """)
            assert_equal(0, len(rs.fetchall()))

        def do_query():
            sql2 = LocalSqlClient(self.root_engine())
            with sql2:
                sql2.execute("""
                    SELECT * FROM mysql.user
                        WHERE User='root'
                        AND Host != 'localhost';
                    """)
        assert_raises_access_denied(do_query)

    @test(depends_on=[generate_root_password, remove_anon_user,
                      remove_remote_root_access])
    def init_mycnf(self):
        """
        Test that it is possible to log in directly under localhost after the
        os_admin user is saved in my.cnf.
        """
        self.prepare.init_mycnf(self.admin_password)
        child = pexpect.spawn("mysql")
        child.expect("mysql>")
        child.sendline("CREATE USER test@'localhost';")
        child.expect("Query OK, 0 rows affected")
        child.sendline(
          "GRANT ALL PRIVILEGES ON *.* TO test@'localhost' WITH GRANT OPTION;")
        child.expect("Query OK, 0 rows affected")
        child.sendline("exit")

    @test(depends_on=[init_mycnf])
    def restart_mysql(self):
        """Test that after restarting the logfiles have increased in size."""
        original_size = os.stat("/var/lib/mysql/ib_logfile0").st_size
        self.prepare.restart_mysql()
        new_size = os.stat("/var/lib/mysql/ib_logfile0").st_size
        if original_size >= new_size:
            fail("The size of the logfile has not increased. "
                 "Old size=" + str(original_size) + ", "
                 "new size=" + str(new_size))
        assert_true(original_size < new_size)
        assert_true(15000000L < new_size);
