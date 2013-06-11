#include "nova/guest/backup.h"

#include "nova/guest/GuestException.h"
#include "nova_guest_version.hpp"
#include <boost/foreach.hpp>
#include "nova/Log.h"
#include <sstream>
#include <string>

using nova::JsonData;
using nova::JsonDataPtr;
using nova::Log;
using nova::guest::GuestException;
using boost::optional;
using std::string;
using std::stringstream;
using namespace boost;

namespace nova { namespace guest { namespace backup {

BackupMessageHandler::BackupMessageHandler(Backup & backup)
: backup(backup) {
}


JsonDataPtr BackupMessageHandler::handle_message(const GuestInput & input) {
    NOVA_LOG_DEBUG("entering the handle_message method now ");
    if (input.method_name == "create_backup") {
        NOVA_LOG_DEBUG("handling the create_backup method");
        const auto id = input.args->get_string("backup_id");
        const auto swift_url = input.args->get_string("swift_url");
        const auto tenant = input.tenant;
        const auto token = input.token;
        backup.run_backup(swift_url, tenant, token, id);
        return JsonData::from_null();
    } else {
        return JsonDataPtr();
    }
}



} } } // end namespace nova::guest::backup
