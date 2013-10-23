#include "pch.hpp"
#include "nova/guest/mysql/MySqlApp.h"
#include "nova/guest/apt.h"
#include <exception>
#include <boost/format.hpp>
#include <fstream>
#include <sstream>
#include "nova/utils/io.h"
//#include <boost/assign/std/list.hpp>
#include <boost/assign/list_of.hpp>
#include <boost/assign/std/list.hpp>
#include "nova/Log.h"
#include "nova/db/mysql.h"
#include "nova/guest/mysql/MySqlGuestException.h"
#include <boost/optional.hpp>
#include "nova/process.h"
#include <sstream>
#include "nova/guest/utils.h"

using nova::guest::apt::AptGuest;
using namespace boost::assign; // brings CommandList += into our code.
using nova::guest::backup::BackupRestoreInfo;
using nova::guest::backup::BackupRestoreManager;
using nova::guest::backup::BackupRestoreManagerPtr;
using nova::utils::io::is_file;
using boost::format;
using namespace nova::guest;
using nova::guest::utils::IsoTime;
using nova::db::mysql::MySqlConnection;
using nova::db::mysql::MySqlConnectionPtr;
using nova::guest::mysql::MySqlGuestException;
using boost::optional;
using namespace nova;
using namespace nova::utils;
using nova::Log;
namespace process = nova::process;
using namespace std;

namespace nova { namespace guest { namespace mysql {

namespace {

    const double TIME_OUT = 500;

    const char * const ADMIN_USER_NAME = "os_admin";
    const char * const ORIG_MYCNF = "/etc/mysql/my.cnf";
    const char * const FINAL_MYCNF ="/var/lib/mysql/my.cnf";
    // There's a permisions issue which necessitates this.
    const char * const HACKY_MYCNF ="/var/lib/nova/my.cnf";
    const char * const TMP_MYCNF = "/tmp/my.cnf.tmp";
    const char * const TRUE_DEBIAN_CNF = "/etc/mysql/debian.cnf";
    const char * const TMP_DEBIAN_CNF = "/tmp/debian.cnf";
    const char * const FRESH_INIT_FILE_PATH = "/tmp/init.sql";
    const char * const FRESH_INIT_FILE_PATH_ARG = "--init-file=/tmp/init.sql";
    const char * const MYCNF_OVERRIDES = "/etc/mysql/conf.d/overrides.cnf";
    const char * const MYCNF_OVERRIDES_TMP = "/tmp/overrides.cnf.tmp";

    void call_update_rc(bool enabled, bool throw_on_bad_exit_code)
    {
        // update-rc.d is typically fast and will return exit code 0 normally,
        // even if it is enabling or disabling mysql twice in a row.
        // UPDATE: Nope, turns out this is only on my box. Sometimes it
        //         returns something else.
        stringstream output;
        process::CommandList cmds = list_of("/usr/bin/sudo")
            ("update-rc.d")("mysql")(enabled ? "enable" : "disable");
        try {
            process::execute(output, cmds);
        } catch(const process::ProcessException & pe) {
            NOVA_LOG_ERROR("Exception running process!");
            NOVA_LOG_ERROR(pe.what());
            if (throw_on_bad_exit_code) {
                NOVA_LOG_ERROR(output.str().c_str());
                throw;
            }
        }
        NOVA_LOG_INFO(output.str().c_str());
    }

    void enable_starting_mysql_on_boot() {
        // Enable MySQL on boot, don't throw if something goes wrong.
        call_update_rc(true, false);
    }

    void guarantee_enable_starting_mysql_on_boot() {
        // Enable MySQL on boot, throw if anything is awry.
        call_update_rc(true, true);
    }

    void disable_starting_mysql_on_boot() {
        // Always throw if the exit code is bad, since disabling this on
        // start up might be needed in case of a reboot.
        call_update_rc(false, true);
    }

    /** Write a new file thats just like the original but with a different
     *  password. */
    void write_temp_mycnf_with_admin_account(const char * temp_file_path,
                                             const string & config_contents,
                                             const char * password) {
        stringstream mycnf_file;

        mycnf_file << config_contents;

        ofstream tmp_file;
        tmp_file.open(temp_file_path);
        if (!tmp_file.good()) {
            NOVA_LOG_ERROR("Couldn't open temp file: %s.", temp_file_path);
            throw MySqlGuestException(MySqlGuestException::CANT_WRITE_TMP_MYCNF);
        }
        string line;
        while(mycnf_file.good()) {
            std::getline(mycnf_file, line);
            tmp_file << line << std::endl;
            if (line.find("[client]", 0) != string::npos) {
                tmp_file << "user\t\t= " << ADMIN_USER_NAME << std::endl;
                tmp_file << "password\t= " << password << std::endl;
            }
        }
        tmp_file.close();
    }
}  // end anonymous namespace

MySqlApp::MySqlApp(MySqlAppStatusPtr status,
                   BackupRestoreManagerPtr backup_restore_manager,
                   int state_change_wait_time,
                   bool skip_install_for_prepare)
:   backup_restore_manager(backup_restore_manager),
    skip_install_for_prepare(skip_install_for_prepare),
    state_change_wait_time(state_change_wait_time),
    status(status)
{
}

MySqlApp::~MySqlApp()
{
}

void MySqlApp::write_config_overrides(const string & overrides_content) {
    NOVA_LOG_INFO("Writing new temp overrides.cnf file.");
    {
        ofstream file(MYCNF_OVERRIDES_TMP, std::ofstream::out);
        if (!file.good()) {
            NOVA_LOG_ERROR("Couldn't open file %s!", MYCNF_OVERRIDES_TMP);
            throw MySqlGuestException(
                MySqlGuestException::CANT_WRITE_TMP_MYCNF);
        }
        file << overrides_content;
    }
    NOVA_LOG_INFO("Moving tmp overrides to final location (%s -> %s).",
                  MYCNF_OVERRIDES_TMP, MYCNF_OVERRIDES);
    process::execute(list_of("/usr/bin/sudo")
                     ("mv")(MYCNF_OVERRIDES_TMP)(MYCNF_OVERRIDES));
    NOVA_LOG_INFO("Setting permissions on %s.", MYCNF_OVERRIDES);
    process::execute(list_of("/usr/bin/sudo")
                     ("chmod")("0711")(MYCNF_OVERRIDES));
}

void MySqlApp::write_mycnf(AptGuest & apt,
                           const string & config_contents,
                           const optional<string> & overrides,
                           const optional<string> & admin_password_arg) {
    NOVA_LOG_INFO("Writing my.cnf templates");
    string admin_password;
    if (admin_password_arg) {
        admin_password = admin_password_arg.get();
    } else {
        // TODO: Maybe in the future, set my.cnf from user_name value?
        string user_name;
        MySqlConnection::get_auth_from_config(
            HACKY_MYCNF, user_name, admin_password);
    }

    /* As of right here, admin_password contains the password to be applied
    to the my.cnf file, whether it was there before (and we passed it in)
    or we generated a new one just now (because we didn't pass it in).
    */

    NOVA_LOG_INFO("Writing info and auth to my.cnf.");
    write_temp_mycnf_with_admin_account(TMP_MYCNF, config_contents,
                                        admin_password.c_str());

    NOVA_LOG_INFO("Copying tmp file so we can log in (permissions work-around).");
    process::execute(list_of("/usr/bin/sudo")("cp")(TMP_MYCNF)(HACKY_MYCNF));
    NOVA_LOG_INFO("Moving tmp into final.");
    process::execute(list_of("/usr/bin/sudo")("mv")(TMP_MYCNF)(FINAL_MYCNF));
    NOVA_LOG_INFO("Removing original my.cnf.");
    process::execute(list_of("/usr/bin/sudo")("rm")("-f")(ORIG_MYCNF));
    NOVA_LOG_INFO("Symlinking final my.cnf.");
    process::execute(list_of("/usr/bin/sudo")("ln")("-s")(FINAL_MYCNF)
                            (ORIG_MYCNF));
    wipe_ib_logfiles();

    if (overrides) {
        write_config_overrides(overrides.get());
    }
}

void MySqlApp::reset_configuration(AptGuest & apt,
                                   const string & config_contents) {
    write_mycnf(apt, config_contents, boost::none, boost::none);
}

void MySqlApp::prepare(AptGuest & apt,
                       const string & config_contents,
                       const optional<string> & overrides,
                       optional<BackupRestoreInfo> restore) {
    // This option allows prepare to be run multiple times. In fact, I'm
    // wondering if this newer version might be safe to just run whenever
    // we want.
    if (!skip_install_for_prepare) {
        if (status->is_mysql_installed()) {
            NOVA_LOG_ERROR("Cannot install and secure MySQL because it is already "
                           "installed.");
            return;
        }
        NOVA_LOG_INFO("Updating status to BUILDING...");
        status->begin_mysql_install();
    } else {
        NOVA_LOG_INFO("Skipping install check.");
    }

    try {
        NOVA_LOG_INFO("Preparing Guest as MySQL Server");
        install_mysql(apt);

        NOVA_LOG_INFO("Waiting until we can connect to MySQL...");
        wait_for_initial_connection();

        NOVA_LOG_INFO("Stopping MySQL to perform additional steps...");
        wait_for_mysql_initial_stop();

        if (restore) {
            NOVA_LOG_INFO("A restore was requested. Running now...");
            backup_restore_manager->run(restore.get());
            NOVA_LOG_INFO("Finished with restore job, proceeding with prepare.");
        }

        NOVA_LOG_INFO("Writing fresh init file...");
        string admin_password = mysql::generate_password();
        write_fresh_init_file(admin_password, !restore);

        NOVA_LOG_INFO("Writing my.cnf...");
        write_mycnf(apt, config_contents, overrides, admin_password);

        NOVA_LOG_INFO("Starting MySQL with init file...");
        run_mysqld_with_init();

        NOVA_LOG_INFO("Erasing init file...");
        if (0 != remove(FRESH_INIT_FILE_PATH)) {
            // Probably not a huge problem in our case.
            NOVA_LOG_ERROR("Can't destroy init file!");
        }

        start_mysql();

        status->end_install_or_restart();
        NOVA_LOG_INFO("Dbaas preparation complete.");
    } catch(const std::exception & e) {
        NOVA_LOG_ERROR("Error installing MySQL!: %s", e.what());
        status->end_failed_install();
        throw;
    }
}

string fetch_debian_sys_maint_password() {
    // Have to copy the debian file to tmp and chown it just to read it. LOL!
    process::execute(list_of("/usr/bin/sudo")("cp")(TRUE_DEBIAN_CNF)
                            (TMP_DEBIAN_CNF));
    process::execute(list_of("/usr/bin/sudo")("/bin/chown")("nova")(TMP_DEBIAN_CNF));
    string user, password;
    MySqlConnection::get_auth_from_config(TMP_DEBIAN_CNF, user, password);
    if (user != "debian-sys-maint") {
        NOVA_LOG_ERROR("Error! user found was not debian-sys-maint but %s!",
                        user.c_str());
        throw MySqlGuestException(MySqlGuestException::DEBIAN_SYS_MAINT_USER_NOT_FOUND);
    }
    return password;
}

void MySqlApp::write_fresh_init_file(const string & admin_password,
                                     bool wipe_root_and_anonymous_users) {
    ofstream init_file(FRESH_INIT_FILE_PATH);

    if (wipe_root_and_anonymous_users) {
        NOVA_LOG_INFO("Generating root password...");
        auto new_root_password = mysql::generate_password();
        init_file << "UPDATE mysql.user SET Password=PASSWORD('"
                  << new_root_password << "') WHERE User='root' AND Host='%';"
                  << std::endl;

        NOVA_LOG_INFO("Removing anonymous users...");
        init_file << "DELETE FROM mysql.user WHERE User='';" << std::endl;

        NOVA_LOG_INFO("Removing root access...");
        init_file << "DELETE FROM mysql.user"
                     "    WHERE User='root'"
                     "    AND Host!='localhost';" << std::endl;
    }

    NOVA_LOG_INFO("Creating admin user...");
    init_file << "GRANT USAGE ON *.* TO '" << ADMIN_USER_NAME
              << "'@'localhost' IDENTIFIED BY '" << admin_password << "';"
              << std::endl;
    init_file << "GRANT ALL PRIVILEGES ON *.* TO '" << ADMIN_USER_NAME
              << "'@'localhost' WITH GRANT OPTION;" << std::endl;

    NOVA_LOG_INFO("Setting debian-sys-maint password.");
    string debian_sys_maint_password = fetch_debian_sys_maint_password();
    init_file << "UPDATE mysql.user SET Password=PASSWORD('"
              << debian_sys_maint_password << "') "
              << "WHERE User='debian-sys-maint';" << std::endl;

    NOVA_LOG_INFO("Flushing privileges");
    init_file << "FLUSH PRIVILEGES;" << std::endl;
}

void MySqlApp::install_mysql(AptGuest & apt) {
    NOVA_LOG_INFO("Installing mysql server.");
    apt.install("mysql-server-5.1", TIME_OUT);
}

void MySqlApp::internal_stop_mysql(bool update_db) {
    NOVA_LOG_INFO("Stopping mysql...");
    process::execute_with_stdout_and_stderr(
        list_of("/usr/bin/sudo")("-E")("/etc/init.d/mysql")("stop"),
        this->state_change_wait_time);
    wait_for_internal_stop(update_db);
}

void MySqlApp::remove_anonymous_user(MySqlAdmin & db) {
    db.get_connection()->query("DELETE FROM mysql.user WHERE User=''");
}

void MySqlApp::remove_overrides() {
    if (is_file(MYCNF_OVERRIDES)) {
        NOVA_LOG_DEBUG("Removing overrides cnf file %s.", MYCNF_OVERRIDES);
        process::execute(list_of("/usr/bin/sudo")
                         ("-f")("rm")(MYCNF_OVERRIDES));
    } else {
        NOVA_LOG_DEBUG("Overrides cnf file %s not found.", MYCNF_OVERRIDES);
    }
}

void MySqlApp::remove_remote_root_access(MySqlAdmin & db) {
    db.get_connection()->query("DELETE FROM mysql.user"
                                   "    WHERE User='root'"
                                   "    AND Host!='localhost'");
    db.get_connection()->flush_privileges();
}

void MySqlApp::restart() {
    struct Restarter {
        MySqlAppStatusPtr & status;

        Restarter(MySqlAppStatusPtr & status)
        :   status(status) {
            status->begin_mysql_restart();
        }

        ~Restarter() {
            // Make sure we end this, even if the result is a failure.
            status->end_install_or_restart();
        }
    } restarter(status);
    internal_stop_mysql();
    enable_starting_mysql_on_boot();
    start_mysql();
}

void MySqlApp::restart_mysql_and_wipe_ib_logfiles() {
    NOVA_LOG_INFO("Restarting mysql...");
    internal_stop_mysql();
    wipe_ib_logfiles();
    enable_starting_mysql_on_boot();
    start_mysql();
}

void MySqlApp::run_mysqld_with_init() {
    NOVA_LOG_INFO("Starting mysqld_safe with init file...");
    process::CommandList cmds = list_of("/usr/bin/sudo")
        ("/usr/bin/mysqld_safe")("--user=mysql")(FRESH_INIT_FILE_PATH_ARG);
    process::Process<> mysqld_safe(cmds);
    if (!status->wait_for_real_state_to_change_to(
        MySqlAppStatus::RUNNING, this->state_change_wait_time, false)) {
        NOVA_LOG_ERROR("Start up of MySQL failed!");
        status->end_install_or_restart();
        throw MySqlGuestException(MySqlGuestException::COULD_NOT_START_MYSQL);
    }
    NOVA_LOG_INFO("Waiting for permissions change.");
    // Wait until we can connect, which establishes that the connection
    // settings were changed.
    int wait_time = 0;
    while(false == MySqlConnection::can_connect_to_localhost_with_mycnf()) {
        NOVA_LOG_INFO("Waiting for user permissions to change.");
        boost::this_thread::sleep(boost::posix_time::seconds(3));
        wait_time += 3;
        if (wait_time > 60) {
            NOVA_LOG_ERROR("Can't connect! Settings never changed!");
            throw MySqlGuestException(MySqlGuestException::CANT_CONNECT_WITH_MYCNF);
        }
    }
    NOVA_LOG_INFO("Killing mysqld_safe...");
    string pid_str = str(format("%d") % mysqld_safe.get_pid());
    process::CommandList kill_cmd = list_of("/usr/bin/sudo")("/bin/kill")
                                           (pid_str.c_str());
    process::execute(kill_cmd);
    NOVA_LOG_INFO("Waiting for process to die...");
    mysqld_safe.wait_for_exit(60);

    wait_for_internal_stop(false);
}

void MySqlApp::start_mysql(bool update_db) {
    NOVA_LOG_INFO("Starting mysql...");
    // As a precaution, make sure MySQL will run on boot.
    process::CommandList cmds = list_of("/usr/bin/sudo")("/etc/init.d/mysql")
                                       ("start");
    process::execute_with_stdout_and_stderr(cmds, this->state_change_wait_time,
                                            false);
    // Wait for MySQL to become pingable. Don't update the database until we're
    // positive of success (this is to follow what's expected by the Trove
    // resize code)
    if (!status->wait_for_real_state_to_change_to(
        MySqlAppStatus::RUNNING, this->state_change_wait_time, false)) {
        NOVA_LOG_ERROR("Start up of MySQL failed!");
        status->end_install_or_restart();
        throw MySqlGuestException(MySqlGuestException::COULD_NOT_START_MYSQL);
    }
    if (update_db) {
        status->wait_for_real_state_to_change_to(
            MySqlAppStatus::RUNNING, this->state_change_wait_time, true);
    }
}

void MySqlApp::start_db_with_conf_changes(AptGuest & apt,
                                          const string & config_contents) {
    NOVA_LOG_INFO("Starting mysql with conf changes...");
    // Restart MySQL and wipe ib logs
    if (status->is_mysql_running()) {
        NOVA_LOG_ERROR("Cannot execute start_db_with_conf_changes because "
            "MySQL state == %s!", status->get_current_status_string());
        throw MySqlGuestException(MySqlGuestException::MYSQL_NOT_STOPPED);
    }
    NOVA_LOG_INFO("Initiating config.");
    // Make sure dbaas package is upgraded during the install in write_mycnf.
    apt.update(TIME_OUT);
    write_mycnf(apt, config_contents, boost::none, boost::none);
    start_mysql(true);
    guarantee_enable_starting_mysql_on_boot();
}

void MySqlApp::stop_db(bool do_not_start_on_reboot) {
    if (do_not_start_on_reboot) {
        disable_starting_mysql_on_boot();
    }
    internal_stop_mysql(true);
}

void MySqlApp::wait_for_initial_connection() {
    NOVA_LOG_INFO("Waiting to get a connection to MySQL...");
    bool can_connect = false;
    while(!can_connect) {
        boost::this_thread::sleep(boost::posix_time::seconds(3));
        MySqlConnection con("localhost", "root", "");
        can_connect = con.test_connection();
    }
}

void MySqlApp::wait_for_mysql_initial_stop() {
    bool stopped = false;
    int attempts = 1;
    while(!stopped) {
        try {
            internal_stop_mysql();
            stopped = true;
        } catch(std::exception & ex) {
            NOVA_LOG_ERROR("Attempt #%d to stop MySQL failed!", attempts);
            ++ attempts;
        }
    }
}

void MySqlApp::wait_for_internal_stop(bool update_db) {
    if (!status->wait_for_real_state_to_change_to(
        MySqlAppStatus::SHUTDOWN, this->state_change_wait_time, update_db)) {
        NOVA_LOG_ERROR("Could not stop MySQL!");
        status->end_install_or_restart();
        throw MySqlGuestException(MySqlGuestException::COULD_NOT_STOP_MYSQL);
    }
}

void MySqlApp::wipe_ib_logfile(const int index) {
    const char * const MYSQL_BASE_DIR = "/var/lib/mysql";
    string logfile = str(format("%s/ib_logfile%d") % MYSQL_BASE_DIR % index);
    process::CommandList cmds = list_of("/usr/bin/sudo")("/bin/ls")
                                       (logfile.c_str());
    process::Process<process::StdErrAndStdOut> proc(cmds);
    proc.wait_for_exit(60);
    if (proc.successful()) {
        process::execute_with_stdout_and_stderr(
            list_of("/usr/bin/sudo")("rm")(logfile.c_str()));
    }
}

void MySqlApp::wipe_ib_logfiles() {
    NOVA_LOG_INFO("Wiping ib_logfiles...");
    // When MySQL starts up, it tries to find log files which are the exact
    // size mandated in its configs. If these do not exist, it simply creates
    // them, but if they do exist and the file is not the same, it fails to
    // start!
    // So we remove the ib_logfile, because if not mysql won't start.
    // For some reason wildcards don't seem to work, so we delete both files
    // separately.
    wipe_ib_logfile(0);
    wipe_ib_logfile(1);
}

} } }  // end nova::guest::mysql
