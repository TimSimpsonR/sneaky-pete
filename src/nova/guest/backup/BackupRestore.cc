#include "pch.hpp"
#include "BackupRestore.h"
#include <boost/assign/list_of.hpp>
#include <boost/assign/std/list.hpp>
#include "nova/Log.h"
#include "nova/process.h"

using namespace boost::assign;

using namespace nova::process;
using std::string;

namespace nova { namespace guest { namespace backup {

BackupRestore::BackupRestore(const string & token, const string & backup_url)
:   backup_url(backup_url),
    token(token)
{
}

void BackupRestore::run() {
    CommandList cmds;
    cmds += "/usr/bin/sudo", "-E", "/var/lib/nova/restore";
    cmds += this->token.c_str();
    cmds += this->backup_url.c_str();
    Process<> proc(cmds);
    proc.wait_forever_for_exit();
    if (!proc.successful()) {
        NOVA_LOG_ERROR("Error running restore process!");
        throw ProcessException(ProcessException::EXIT_CODE_NOT_ZERO);
    }
}


} } } // end namespace nova::guest::backup
