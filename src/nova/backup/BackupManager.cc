#include "pch.hpp"
#include "nova/backup/BackupException.h"
#include "nova/backup/BackupManager.h"
#include <boost/format.hpp>
#include <fstream>
#include <iostream>
#include <boost/lexical_cast.hpp>
#include <boost/assign/list_of.hpp>
#include <boost/assign/std/list.hpp>
#include "nova/Log.h"
#include "nova/process.h"
#include "nova/utils/Curl.h"
#include <sstream>
#include <string>
#include <sys/statvfs.h>
#include <curl/curl.h>
#include "nova/utils/swift.h"
#include "nova/guest/utils.h"
#include "nova/utils/zlib.h"
#include "nova/utils/subsecond.h"

using namespace boost::assign;
using nova::process::CommandList;
using nova::process::IndependentStdErrAndStdOut;
using nova::process::Process;
using std::string;
using namespace boost;
using namespace std;
using nova::utils::Curl;
using nova::utils::CurlScope;
using nova::guest::utils::IsoDateTime;
using nova::guest::diagnostics::Interrogator;
using nova::utils::Job;
using nova::utils::JobRunner;
using nova::utils::swift::SwiftClient;
using nova::utils::swift::SwiftFileInfo;
using nova::utils::swift::SwiftUploader;
using namespace nova::guest::diagnostics;
using nova::rpc::ResilientSenderPtr;
using nova::utils::subsecond::now;

namespace zlib = nova::utils::zlib;

namespace nova { namespace backup {


/**---------------------------------------------------------------------------
 *- BackupJob
 *---------------------------------------------------------------------------*/

namespace {
    const char * const BUILDING = "BUILDING";
    const char * const FAILED = "FAILED";
}

BackupJob::BackupJob(const BackupRunnerData & data,
                     const BackupCreationArgs & args)
:   args(args),
    data(data),
    file_info(args.location, data.swift_container, args.id) {
}

BackupJob::~BackupJob() {
}

const char * BackupJob::status_name(const BackupJob::Status status) {
    switch(status) {
        //! BEGIN GENERATED CODE
        case BUILDING:
            return "BUILDING";
        case COMPLETED:
            return "COMPLETED";
        case FAILED:
            return "FAILED";
        //! END GENERATED CODE
        default:
            return "Invalid status code!";
    }
}

void BackupJob::update_trove_to_building() {
    update_trove(BUILDING, boost::none);
}

void BackupJob::update_trove_to_failed() {
    update_trove(FAILED, boost::none);
}

void BackupJob::update_trove_to_completed(const string & checksum) {
    update_trove(COMPLETED, checksum);
}

void BackupJob::update_trove(const BackupJob::Status status,
                             const optional<string> & checksum) {
    FileSystemStatsPtr stats = data.interrogator.get_mount_point_stats();
    const IsoDateTime iso_now;
    JsonObjectBuilder update_args;
    update_args.add("backup_id", args.id,
             "backup_type", this->get_backup_type(),
             "checksum", checksum.get_value_or(""),
             "location", this->file_info.manifest_url(),
             "size", stats->used,
             "state", status_name(status),
             "updated", iso_now.c_str());
    if (checksum) {
        update_args.add("checksum", checksum.get());
    }
    data.sender->send("update_backup", update_args);
    NOVA_LOG_INFO("Updating backup %s to state %s", args.id.c_str(),
                   status_name(status));
    NOVA_LOG_DEBUG("Time: %s, Volume used: %.2f", iso_now.c_str(), stats->used);
}


/**---------------------------------------------------------------------------
 *- BackupManagerInfo
 *---------------------------------------------------------------------------*/

BackupManagerInfo::BackupManagerInfo(
    BackupRunnerData data,
    JobRunner & runner)
:   data(data),
    runner(runner)
{
}

BackupManagerInfo::BackupManagerInfo(const BackupManagerInfo & info)
:   data(info.data),
    runner(info.runner)
{
}

BackupManagerInfo::~BackupManagerInfo() {
}


/**---------------------------------------------------------------------------
 *- BackupManager
 *---------------------------------------------------------------------------*/

BackupManager::BackupManager(const BackupManagerInfo & info)
: BackupManagerInfo(info)
{
}


} } // end namespace nova::backup
