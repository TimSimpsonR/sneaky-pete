#include "pch.hpp"
#include "nova/guest/backup/BackupException.h"
#include "nova/guest/backup/BackupManager.h"
#include "nova/guest/backup/BackupMessageHandler.h"
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

namespace zlib = nova::utils::zlib;

namespace nova { namespace guest { namespace backup {



/**---------------------------------------------------------------------------
 *- BackupManagerInfo
 *---------------------------------------------------------------------------*/

BackupManagerInfo::BackupManagerInfo(
    ResilientSenderPtr sender,
    JobRunner & runner,
    const Interrogator interrogator,
    const int segment_max_size,
    const int checksum_wait_time,
    const string swift_container,
    const double time_out,
    const int zlib_buffer_size)
:   sender(sender),
    runner(runner),
    interrogator(interrogator),
    segment_max_size(segment_max_size),
    checksum_wait_time(checksum_wait_time),
    swift_container(swift_container),
    time_out(time_out),
    zlib_buffer_size(zlib_buffer_size)
{
}

BackupManagerInfo::BackupManagerInfo(const BackupManagerInfo & info)
:   sender(info.sender),
    runner(info.runner),
    interrogator(info.interrogator),
    segment_max_size(info.segment_max_size),
    checksum_wait_time(info.checksum_wait_time),
    swift_container(info.swift_container),
    time_out(info.time_out),
    zlib_buffer_size(info.zlib_buffer_size)
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


} } } // end namespace nova::guest::backup
