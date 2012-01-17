#include "nova/guest/mysql/MySqlApp.h"
#include "nova/guest/apt.h"
#include <boost/format.hpp>
#include <fstream>
#include "nova/utils/io.h"
//#include <boost/assign/std/list.hpp>
#include <boost/assign/list_of.hpp>
#include "nova/Log.h"
#include "nova/db/mysql.h"
#include "nova/guest/mysql/MySqlGuestException.h"
#include "nova/process.h"
#include "nova/guest/utils.h"

using nova::guest::apt::AptGuest;
using namespace boost::assign; // brings CommandList += into our code.
using boost::format;
using namespace nova::guest;
using nova::guest::utils::IsoTime;
using nova::db::mysql::MySqlConnection;
using nova::db::mysql::MySqlConnectionPtr;
using nova::guest::mysql::MySqlGuestException;
using namespace nova;
using namespace nova::utils;
using nova::Log;
using nova::Process;
using namespace std;

namespace nova { namespace guest { namespace mysql {

namespace {

    const double TIME_OUT = 500;

    const char * ADMIN_USER_NAME = "os_admin";
    const char * ORIG_MYCNF = "/etc/mysql/my.cnf";
    const char * FINAL_MYCNF ="/var/lib/mysql/my.cnf";
    // There's a permisions issue which necessitates this.
    const char * HACKY_MYCNF ="/var/lib/nova/my.cnf";
    const char * TMP_MYCNF = "/tmp/my.cnf.tmp";
    const char * DBAAS_MYCNF = "/etc/dbaas/my.cnf/my.cnf.default";

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
            NOVA_LOG_ERROR2("Couldn't open temp file: %s.", temp_file_path);
            throw MySqlGuestException(MySqlGuestException::CANT_WRITE_TMP_MYCNF);
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

MySqlApp::MySqlApp(MySqlAppStatusPtr status, int state_change_wait_time)
:   state_change_wait_time(state_change_wait_time),
    status(status)
{
}

MySqlApp::~MySqlApp()
{
}

void MySqlApp::create_admin_user(MySqlAdminPtr & db,
                                 const std::string & password) {
    MySqlUserPtr user(new MySqlUser());
    user->set_name(ADMIN_USER_NAME);
    user->set_password(password);
    db->create_user(user, "localhost");
    db->get_connection()->grant_all_privileges(ADMIN_USER_NAME, "localhost");
}

void MySqlApp::generate_root_password(MySqlAdminPtr & db) {
    db->set_password("root", mysql::generate_password().c_str());
}

void MySqlApp::init_mycnf(AptGuest & apt, const std::string & password) {
    NOVA_LOG_INFO("Installing my.cnf templates");

    apt.install("dbaas-mycnf", TIME_OUT);

    NOVA_LOG_INFO("Replacing my.cnf with template.");
    replace_mycnf_with_template(DBAAS_MYCNF, ORIG_MYCNF);

    NOVA_LOG_INFO("Writing new temp my.cnf.");
    write_temp_mycnf_with_admin_account(ORIG_MYCNF, TMP_MYCNF, password.c_str());

    NOVA_LOG_INFO("Copying tmp file so we can log in (permissions work-around).");
    Process::execute(list_of("/usr/bin/sudo")("cp")(TMP_MYCNF)(HACKY_MYCNF));
    NOVA_LOG_INFO("Moving tmp into final.");
    Process::execute(list_of("/usr/bin/sudo")("mv")(TMP_MYCNF)(FINAL_MYCNF));
    NOVA_LOG_INFO("Removing original my.cnf.");
    Process::execute(list_of("/usr/bin/sudo")("rm")(ORIG_MYCNF));
    NOVA_LOG_INFO("Symlinking final my.cnf.");
    Process::execute(list_of("/usr/bin/sudo")("ln")("-s")(FINAL_MYCNF)
                            (ORIG_MYCNF));
}

MySqlAdminPtr MySqlApp::install_and_secure(AptGuest & apt) {
    // The MySqlAdmin class is lazy initialized, so we can create it before
    // the app exists.
    MySqlConnectionPtr con(new MySqlConnection("localhost", "root", ""));
    MySqlAdminPtr local_db(new MySqlAdmin(con));

    if (status->is_mysql_installed()) {
        NOVA_LOG_ERROR("Cannot install and secure MySQL because it is already "
                       "installed.");
        return local_db;
    }
    NOVA_LOG_INFO("Updating status to BUILDING...");
    status->begin_mysql_install();

    // You may wonder why there's no try / catch around this section which
    // would change status to a failure state if something went wrong.
    // The reason why is that the Reddwarf compute manager will time out the
    // instance. If we added a catch here, it would be harder to test this
    // ability of the Reddwarf compute manager. We may find a way to add error
    // handling here anyway as it would speed up acknowledgement of failures,
    // but the bigger priority is making sure the manager retains the ability
    // to determine when a guest is misbehaving.

    NOVA_LOG_INFO("Preparing Guest as MySQL Server");
    install_mysql(apt);

    string admin_password = mysql::generate_password();

    NOVA_LOG_INFO("Generating root password...");
    generate_root_password(local_db);
    NOVA_LOG_INFO("Removing anonymous users...");
    remove_anonymous_user(local_db);
    NOVA_LOG_INFO("Removing root access...");
    remove_remote_root_access(local_db);
    NOVA_LOG_INFO("Creating admin user...");
    create_admin_user(local_db, admin_password);
    NOVA_LOG_INFO("Flushing privileges");
    local_db->get_connection()->flush_privileges();

    NOVA_LOG_INFO("Initiating config.");
    init_mycnf(apt, admin_password);
    local_db->get_connection()->close();
    NOVA_LOG_INFO("Restarting MySQL...");
    restart_mysql_and_wipe_ib_logfiles();

    status->end_install_or_restart();
    NOVA_LOG_INFO("Dbaas preparation complete.");
    return local_db;
}

void MySqlApp::install_mysql(AptGuest & apt) {
    NOVA_LOG_INFO("Installing mysql server.");
    apt.install("mysql-server-5.1", TIME_OUT);
}

void MySqlApp::remove_anonymous_user(MySqlAdminPtr & db) {
    db->get_connection()->query("DELETE FROM mysql.user WHERE User=''");
}

void MySqlApp::remove_remote_root_access(MySqlAdminPtr & db) {
    db->get_connection()->query("DELETE FROM mysql.user"
                                   "    WHERE User='root'"
                                   "    AND Host!='localhost'");
    db->get_connection()->flush_privileges();
}

void MySqlApp::restart() {
    status->begin_mysql_restart();
    stop_mysql();
    start_mysql();
    status->end_install_or_restart();
}

void MySqlApp::restart_mysql_and_wipe_ib_logfiles() {
    const char * MYSQL_BASE_DIR = "/var/lib/mysql";
    NOVA_LOG_INFO("Restarting mysql...");

    stop_mysql();

    NOVA_LOG_INFO("Wiping ib_logfiles...");
    // When MySQL starts up, it tries to find log files which are the exact
    // size mandated in its configs. If these do not exist, it simply creates
    // them, but if they do exist and the file is not the same, it fails to
    // start!
    // So we remove the ib_logfile, because if not mysql won't start.
    // For some reason wildcards don't seem to work, so we delete both files
    // separately.
    string logfile0 = str(format("%s/ib_logfile0") % MYSQL_BASE_DIR);
    string logfile1 = str(format("%s/ib_logfile1") % MYSQL_BASE_DIR);
    Process::execute(list_of("/usr/bin/sudo")("rm")(logfile0.c_str()));
    Process::execute(list_of("/usr/bin/sudo")("rm")(logfile1.c_str()));

    start_mysql();
}

void MySqlApp::start_mysql() {
    NOVA_LOG_INFO("Starting mysql...");
    Process::execute(list_of("/usr/bin/sudo")("/etc/init.d/mysql")("start"));
    if (!status->wait_for_real_state_to_change_to(
        MySqlAppStatus::RUNNING, this->state_change_wait_time)) {
        NOVA_LOG_ERROR("Start up of MySQL failed!");
        status->end_install_or_restart();
        throw MySqlGuestException(MySqlGuestException::COULD_NOT_START_MYSQL);
    }
}

void MySqlApp::stop_mysql() {
    NOVA_LOG_INFO("Stopping mysql...");
    Process::execute(list_of("/usr/bin/sudo")("/etc/init.d/mysql")("stop"));
    if (!status->wait_for_real_state_to_change_to(
        MySqlAppStatus::SHUTDOWN, this->state_change_wait_time)) {
        NOVA_LOG_ERROR("Could not stop MySQL!");
        status->end_install_or_restart();
        throw MySqlGuestException(MySqlGuestException::COULD_NOT_STOP_MYSQL);
    }
}

} } }  // end nova::guest::mysql
