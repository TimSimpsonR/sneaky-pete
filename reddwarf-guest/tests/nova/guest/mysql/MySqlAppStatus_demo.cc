#include "nova/guest/utils.h"
#include <boost/format.hpp>
#include "nova/flags.h"
#include <iostream>
#include "nova/Log.h"
#include "nova/db/mysql.h"
#include "nova/guest/mysql/MySqlAppStatus.h"
#include <string>


using namespace nova;
using namespace nova::flags;
using boost::format;
using namespace nova::db::mysql;
using namespace nova::guest::mysql;
using std::string;
using namespace std;
namespace utils = nova::guest::utils;

struct nova::guest::mysql::MySqlAppStatusTestsFixture {
    static void demo(int argc, char* argv[]) {
        LogApiScope log_api_scope(LogOptions::simple());
        FlagValues flags(FlagMap::create_from_args(argc, argv, true));

        MySqlConnectionWithDefaultDbPtr sql_connection(
            new MySqlConnectionWithDefaultDb(
            flags.nova_sql_host(), flags.nova_sql_user(),
            flags.nova_sql_password(), "nova"));

        cout << "Host Name = " << utils::get_host_name() << endl;

        string value = utils::get_ipv4_address(flags.guest_ethernet_device());
        cout << "Ip V4 Address = " << value << endl;

        MySqlAppStatus updater(sql_connection,
                               flags.guest_ethernet_device(),
                               flags.nova_db_reconnect_wait_time());

        cout << "Instance ID = " << updater.get_guest_instance_id() << endl;

        cout << "Setting state to 'CRASHED'." << endl;

        updater.set_status(MySqlAppStatus::CRASHED);

        cout << "Current status of local db is "
             << updater.status_name(updater.get_actual_db_status()) << "."
             << endl;
    }

};

// Put this into a container to see if it can grab the correct information.
int main(int argc, char* argv[]) {
    nova::guest::mysql::MySqlAppStatusTestsFixture::demo(argc, argv);
}
