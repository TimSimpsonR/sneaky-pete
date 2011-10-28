#include "nova/guest/mysql/MySqlPreparer.h"
#include "nova/guest/apt.h"
#include <boost/format.hpp>
#include <fstream>
#include "nova/utils/io.h"
//#include <boost/assign/std/list.hpp>
#include <boost/assign/list_of.hpp>
#include "nova/Log.h"
#include "nova/process.h"

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
    const char * TMP_MYCNF = "/tmp/my.cnf.tmp";
    const char * DBAAS_MYCNF = "/etc/dbaas/my.cnf/my.cnf.default";

    class IsoTime {
    private:
        char str[11];

    public:
        IsoTime() {
            time_t t = time(NULL);
            tm * tmp = localtime(&t);
            if (tmp == NULL) {
                log.debug("Could not get localtime!");
                delete tmp;
                throw MySqlException(MySqlException::GENERAL);
            }
            if (strftime(str, sizeof(str), "%Y-%m-%d", tmp) == 0) {
                log.debug("strftime returned 0");
                delete tmp;
                throw MySqlException(MySqlException::GENERAL);
            }
            delete tmp;
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
    void replace_mycnf_with_template(const char * original_path,
                                     const char * template_path) {
        if (io::is_file(template_path)) {
            IsoTime time;
            string new_mycnf = str(format("%s.%s") % original_path
                                   % time.c_str());
            Process::execute(list_of("sudo")("mv")(original_path)
                             (new_mycnf.c_str()));
            Process::execute(list_of("sudo")("cp")(template_path)
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
            log.debug("Couldn't open temp file: %s.", temp_file_path);
        }
        string line;
        while(!mycnf_file.eof()) {
            std::getline(mycnf_file, line);
            if (line.find("[client]", 0) != string::npos) {
                tmp_file << "user\t\t= " << ADMIN_USER_NAME << std::endl;
                tmp_file << "password\t= " << password << std::endl;
            }
        }
        mycnf_file.close();
        tmp_file.close();
    }
}

MySqlPreparer::MySqlPreparer(MySqlGuestPtr guest)
: guest(guest)
{
}

void MySqlPreparer::create_admin_user(const std::string & password) {
    MySqlUserPtr user(new MySqlUser());
    user->set_name(ADMIN_USER_NAME);
    user->set_password(password);
    guest->create_user(user, "localhost");
    guest->get_connection()->grant_all_privileges(ADMIN_USER_NAME, "localhost");
}

void MySqlPreparer::generate_root_password() {
    guest->set_password("root", mysql::generate_password().c_str());
}

void MySqlPreparer::init_mycnf(const std::string & password) {
    log.debug("Installing my.cnf templates");

    apt::install("dbaas-mycnf", TIME_OUT);

    replace_mycnf_with_template(ORIG_MYCNF, DBAAS_MYCNF);

    write_temp_mycnf_with_admin_account(ORIG_MYCNF, TMP_MYCNF, password.c_str());

    Process::execute(list_of("sudo")("mv")(TMP_MYCNF)(FINAL_MYCNF));
    Process::execute(list_of("sudo")("rm")(ORIG_MYCNF));
    Process::execute(list_of("sudo")("ln")("-s")(FINAL_MYCNF)(ORIG_MYCNF));
}

void MySqlPreparer::install_mysql() {
    Log log;
    log.debug("Installing mysql server.");
    apt::install("mysql-server-5.1", TIME_OUT);
}

void MySqlPreparer::prepare() {
    log.debug("Preparing Guest as MySQL Server");
    install_mysql();
    string admin_password = mysql::generate_password();

    generate_root_password();
    remove_anonymous_user();
    remove_remote_root_access();
    create_admin_user(admin_password);
    guest->get_connection()->flush_privileges();

    init_mycnf(admin_password);
    restart_mysql();

    log.debug("Dbaas preparation complete.");
}

void MySqlPreparer::remove_anonymous_user() {
    guest->get_connection()->query("DELETE FROM mysql.user WHERE User=''");
}

void MySqlPreparer::remove_remote_root_access() {
    guest->get_connection()->query("DELETE FROM mysql.user"
                                   "    WHERE User='root'"
                                   "    AND Host!='localhost'");
    guest->get_connection()->flush_privileges();
}

void MySqlPreparer::restart_mysql() {
    //TODO(rnirmal): To be replaced by the mounted volume location
    //FIXME once we have volumes in place, use default till then
    const char * MYSQL_BASE_DIR = "/var/lib/mysql";
    log.debug("Restarting mysql...");
    Process::execute(list_of("sudo")("service")("mysql")("stop"));

    // Remove the ib_logfile, if not mysql won't start.
    // For some reason wildcards don't seem to work, so
    // deleting both the files separately
    string logfile0 = str(format("%s/ib_logfile0") % MYSQL_BASE_DIR);
    Process::execute(list_of("sudo")("rm")(logfile0.c_str()));
    Process::execute(list_of("sudo")("service")("mysql")("start"));
}

} } }  // end nova::guest::mysql
