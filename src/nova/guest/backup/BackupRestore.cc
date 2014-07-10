#include "pch.hpp"
#include "BackupRestore.h"
#include "nova/Log.h"

using std::string;


namespace nova { namespace guest { namespace backup {


/**---------------------------------------------------------------------------
 *- BackupRestoreInfo
 *---------------------------------------------------------------------------*/

BackupRestoreInfo::BackupRestoreInfo(const string & token,
                                     const string & backup_url,
                                     const string & backup_checksum)
:   token(token),
    backup_url(backup_url),
    backup_checksum(backup_checksum) {
}

/**---------------------------------------------------------------------------
 *- BackupRestoreManager
 *---------------------------------------------------------------------------*/

BackupRestoreManager::BackupRestoreManager() {
}

BackupRestoreManager::~BackupRestoreManager() {
}

/**----------------------------------------------   -----------------------------
 *- BackupRestoreException
 *---------------------------------------------------------------------------*/

const char * BackupRestoreException::what() const throw() {
    return "Error performing backup restore!";
}


} } } // end namespace nova::guest::backup
