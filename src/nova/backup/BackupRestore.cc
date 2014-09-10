#include "pch.hpp"
#include "BackupRestore.h"
#include "nova/Log.h"

using std::string;


namespace nova { namespace backup {


/**---------------------------------------------------------------------------
 *- BackupRestoreInfo
 *---------------------------------------------------------------------------*/

BackupRestoreInfo::BackupRestoreInfo(const string & token,
                                     const string & location,
                                     const string & checksum)
:   token(token),
    location(location),
    checksum(checksum) {
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


} } // end namespace nova::guest::backup
