#include "nova/guest/mysql/MySqlPreparer.h"
#include "nova/guest/apt.h"
#include <boost/format.hpp>
#include <fstream>
#include "nova/utils/io.h"
//#include <boost/assign/std/list.hpp>
#include <boost/assign/list_of.hpp>
#include "nova/Log.h"
#include "nova/process.h"

using nova::guest::apt::AptGuest;
using namespace boost::assign; // brings CommandList += into our code.
using boost::format;
using namespace nova::guest;
using namespace nova;
using namespace nova::utils;
using nova::Log;
using sql::PreparedStatement;
using nova::Process;
using sql::ResultSet;
using sql::SQLException;
using namespace std;

namespace nova { namespace guest { namespace mysql {

namespace {
    Log log;
    const double TIME_OUT = 500;

    const char * ADMIN_USER_NAME = "os_admin";
    const char * ORIG_MYCNF = "/etc/mysql/my.cnf";
    const char * FINAL_MYCNF ="/var/lib/mysql/my.cnf";
    // There's a permisions issue which necessitates this.
    const char * HACKY_MYCNF ="/var/lib/nova/my.cnf";
    const char * TMP_MYCNF = "/tmp/my.cnf.tmp";
    const char * DBAAS_MYCNF = "/etc/dbaas/my.cnf/my.cnf.default";

    class IsoTime {
    private:
        char str[11];

    public:
        IsoTime() {
            time_t t = time(NULL);
            // The returned pointer is statically allocated and shared, so
            // don't free it.
            tm * tmp = localtime(&t);
            if (tmp == NULL) {
                log.error("Could not get localtime!");
                throw MySqlException(MySqlException::GENERAL);
            }
            if (strftime(str, sizeof(str), "%Y-%m-%d", tmp) == 0) {
                log.error("strftime returned 0");
                throw MySqlException(MySqlException::GENERAL);
            }
        }

        const char * c_str() const {
            return str;
        }
    };
    const char * blah() {
        return "hnfgh";
    }
    /** If there is a file at template_path, back up the current file to a new
     *  file ending with today's date, and then copy the template file over
     *  it. */
    void replace_mycnf_with_template(const char * template_path,
                                     const char * original_path) {
        if (io::is_file(template_path)) {
            IsoTime time;
            string new_mycnf = str(format("%s.%s") % original_path
                                   % time.c_str());
            Process::execute(list_of("/usr/bin/sudo")("mv")(original_path)
                             (new_mycnf.c_str()));
            Process::execute(list_of("/usr/bin/sudo")("cp")(template_path)
                             (original_path));
        }
    }

    /** Write a new file thats just like the original but with a different
     *  password. */
    void write_temp_mycnf_with_admin_account(const char * original_file_path,
                                             const char * temp_file_path,
                                             const char * password) {
        ifstream mycnf_file;
        mycnf_file.open(original_file_path);
        ofstream tmp_file;
        tmp_file.open(temp_file_path);
        if (!tmp_file.good()) {
            log.error2("Couldn't open temp file: %s.", temp_file_path);
            throw MySqlException(MySqlException::CANT_WRITE_TMP_MYCNF);
        }
        string line;
        while(!mycnf_file.eof()) {
            std::getline(mycnf_file, line);
            tmp_file << line << std::endl;
            if (line.find("[client]", 0) != string::npos) {
                tmp_file << "user\t\t= " << ADMIN_USER_NAME << std::endl;
                tmp_file << "password\t= " << password << std::endl;
            }
        }
        mycnf_file.close();
        tmp_file.close();
    }
}

MySqlPreparer::MySqlPreparer(AptGuest * apt)
: apt(apt), sql()
{
    MySqlConnectionPtr con(new MySqlConnection("localhost", "root", ""));
    sql.reset(new MySqlGuest(con));
}

void MySqlPreparer::create_admin_user(const std::string & password) {
    MySqlUserPtr user(new MySqlUser());
    user->set_name(ADMIN_USER_NAME);
    user->set_password(password);
    sql->create_user(user, "localhost");
    sql->get_connection()->grant_all_privileges(ADMIN_USER_NAME, "localhost");
}

void MySqlPreparer::generate_root_password() {
    sql->set_password("root", mysql::generate_password().c_str());
}

void MySqlPreparer::init_mycnf(const std::string & password) {
    log.info("Installing my.cnf templates");

    apt->install("dbaas-mycnf", TIME_OUT);

    log.info("Replacing my.cnf with template.");
    replace_mycnf_with_template(DBAAS_MYCNF, ORIG_MYCNF);

    log.info("Writing new temp my.cnf.");
    write_temp_mycnf_with_admin_account(ORIG_MYCNF, TMP_MYCNF, password.c_str());

    log.info("Copying tmp file so we can log in (permissions work-around).");
    Process::execute(list_of("/usr/bin/sudo")("cp")(TMP_MYCNF)(HACKY_MYCNF));
    log.info("Moving tmp into final.");
    Process::execute(list_of("/usr/bin/sudo")("mv")(TMP_MYCNF)(FINAL_MYCNF));
    log.info("Removing original my.cnf.");
    Process::execute(list_of("/usr/bin/sudo")("rm")(ORIG_MYCNF));
    log.info("Symlinking final my.cnf.");
    Process::execute(list_of("/usr/bin/sudo")("ln")("-s")(FINAL_MYCNF)
                            (ORIG_MYCNF));
}

void MySqlPreparer::install_mysql() {
    Log log;
    log.info("Installing mysql server.");
    apt->install("mysql-server-5.1", TIME_OUT);
}

void MySqlPreparer::prepare() {
    log.info("Preparing Guest as MySQL Server");
    install_mysql();
    string admin_password = mysql::generate_password();

    log.info("Generating root password...");
    generate_root_password();
    log.info("Removing anonymous users...");
    remove_anonymous_user();
    log.info("Removing root access...");
    remove_remote_root_access();
    log.info("Creating admin user...");
    create_admin_user(admin_password);
    log.info("Flushing privileges");
    sql->get_connection()->flush_privileges();

    log.info("Initiating config.");
    init_mycnf(admin_password);
    sql->get_connection()->close();
    log.info("Restarting MySQL...");
    restart_mysql();

    log.info("Dbaas preparation complete.");
}

void MySqlPreparer::remove_anonymous_user() {
    sql->get_connection()->query("DELETE FROM mysql.user WHERE User=''");
}

void MySqlPreparer::remove_remote_root_access() {
    sql->get_connection()->query("DELETE FROM mysql.user"
                                   "    WHERE User='root'"
                                   "    AND Host!='localhost'");
    sql->get_connection()->flush_privileges();
}

void MySqlPreparer::restart_mysql() {
    const char * MYSQL_BASE_DIR = "/var/lib/mysql";
    log.info("Restarting mysql...");
    Process::execute(list_of("/usr/bin/sudo")("service")("mysql")("stop"));

    // Remove the ib_logfile, if not mysql won't start.
    // For some reason wildcards don't seem to work, so
    // deleting both the files separately
    string logfile0 = str(format("%s/ib_logfile0") % MYSQL_BASE_DIR);
    string logfile1 = str(format("%s/ib_logfile1") % MYSQL_BASE_DIR);
    Process::execute(list_of("/usr/bin/sudo")("rm")(logfile0.c_str()));
    Process::execute(list_of("/usr/bin/sudo")("rm")(logfile1.c_str()));
    Process::execute(list_of("/usr/bin/sudo")("service")("mysql")("start"));
}

} } }  // end nova::guest::mysql
