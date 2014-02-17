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
#include "nova/guest/diagnostics.h"
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
using nova::utils::Job;
using nova::utils::JobRunner;
using nova::utils::swift::SwiftClient;
using nova::utils::swift::SwiftFileInfo;
using nova::utils::swift::SwiftUploader;
using namespace nova::guest::diagnostics;
using nova::rpc::ResilientSenderPtr;
using nova::utils::subsecond::now;

namespace zlib = nova::utils::zlib;

namespace nova { namespace guest { namespace backup {

namespace {  // Begin anonymous namespace

/**---------------------------------------------------------------------------
 *- BackupProcessReader
 *---------------------------------------------------------------------------*/


/* Checks that the last bit of output is the XtraBackup success message. */
class CabooseChecker {
    public:
        CabooseChecker()
        :   success_string("completed OK!"),
            desired_length(success_string.length() + 2),  // Add wiggle room.
            trail()
        {
        }

        inline bool successful() const {
            return string::npos != trail.find(success_string);
        }

        inline void write(const char * const buffer, const size_t length) {
            if (length >= desired_length) {
                const size_t diff = length - desired_length;
                const char * start = buffer + diff;
                trail.replace(trail.begin(), trail.end(),
                              start, desired_length);
            } else {
                trail.append(buffer, length);
                const signed int diff = trail.size() - desired_length;
                if (diff > 0) {
                    trail.erase(0, diff);
                }
            }
        }
    private:
        const string success_string;
        const size_t desired_length;
        string trail;
};


/* Calls xtrabackup and presents an interface to be used as a zlib source. */
class XtraBackupReader : public zlib::InputStream {
public:
    XtraBackupReader(CommandList cmds, size_t zlib_buffer_size,
        optional<double> time_out)
    :   buffer(new char [zlib_buffer_size]),
        last_stdout_write_length(0),
        process(cmds),
        zlib_buffer_size(zlib_buffer_size),
        time_out(time_out),
        xtrabackup_log()
    {
        xtrabackup_log.open("/tmp/xtrabackup.log");
    }

    virtual ~XtraBackupReader() {
        delete[] buffer;
    }

    /** Reads from the process until getting new STDOUT. */
    virtual zlib::ZlibBufferStatus advance() {
        optional<zlib::InputStream> stdout;
        while(true) {
            if (process.is_finished()) {
                return zlib::FINISHED;
            }
            const auto result = process.read_into(buffer, zlib_buffer_size,
                                                  time_out);
            if (result.err()) {
                caboose.write(buffer, result.write_length);
                xtrabackup_log.write(buffer, result.write_length);
            } else if (result.out()) {
                last_stdout_write_length = result.write_length;
                return zlib::OK;
            } else {
                NOVA_LOG_ERROR("Time out while looking for output from backup "
                               "process. Reading again...");
            }
        }
    }

    virtual char * get_buffer() {
        return buffer;
    }

    virtual size_t get_buffer_size() {
        return last_stdout_write_length;
    }

    bool successful() const {
        return process.successful() && caboose.successful();
    }

private:
    char* buffer;
    CabooseChecker caboose;
    size_t last_stdout_write_length;
    Process<IndependentStdErrAndStdOut> process;
    size_t zlib_buffer_size;
    optional<double> time_out;
    std::ofstream xtrabackup_log;
};


typedef boost::shared_ptr<XtraBackupReader> XtraBackupReaderPtr;


class BackupProcessReader : public SwiftUploader::Input {
public:

    BackupProcessReader(CommandList cmds, size_t zlib_buffer_size, optional<double> time_out)
    :   process(new XtraBackupReader(cmds, zlib_buffer_size, time_out)),
        compressor()
    {
    }

    virtual ~BackupProcessReader() {
    }

    virtual bool successful() const {
        return process->successful();
    }

    virtual bool eof() const {
        return compressor.is_finished();
    }

    virtual size_t read(char * buffer, size_t bytes) {
        return compressor.run_write_into(process, buffer, bytes);
    }

private:

    XtraBackupReaderPtr process;
    zlib::ZlibCompressor compressor;
};


/**---------------------------------------------------------------------------
 *- BackupJob
 *---------------------------------------------------------------------------*/

/** Represents an instance of a in-flight backup. */
class BackupJob : public nova::utils::Job {
public:
    BackupJob(
        ResilientSenderPtr sender,
        const CommandList commands,
        const int & segment_max_size,
        const string & swift_container,
        const double time_out,
        const string & tenant,
        const string & token,
        const size_t & zlib_buffer_size,
        const BackupInfo & backup_info)
    :   backup_info(backup_info),
        sender(sender),
        commands(commands),
        segment_max_size(segment_max_size),
        swift_container(swift_container),
        tenant(tenant),
        time_out(time_out),
        token(token),
        zlib_buffer_size(zlib_buffer_size) {
    }

    // Copy constructor is designed mainly so we can pass instances
    // from one thread to another.
    BackupJob(const BackupJob & other)
    :   backup_info(other.backup_info),
        sender(other.sender),
        commands(other.commands),
        segment_max_size(other.segment_max_size),
        swift_container(other.swift_container),
        tenant(other.tenant),
        time_out(other.time_out),
        token(other.token),
        zlib_buffer_size(other.zlib_buffer_size) {
    }

    ~BackupJob() {
    }

    virtual void operator()() {
        NOVA_LOG_INFO("Starting backup...");
        try {
            // Start process
            // As we read, write to Swift
            dump();
            NOVA_LOG_INFO("Backup completed without throwing errors.");
        } catch(const std::exception & ex) {
            NOVA_LOG_ERROR("Error running backup!");
            NOVA_LOG_ERROR("Exception: %s", ex.what());
            update_db("FAILED");
        } catch(...) {
            NOVA_LOG_ERROR("Error running backup!");
            update_db("FAILED");
        }
    }

    virtual Job * clone() const {
        return new BackupJob(*this);
    }

private:
    // Don't allow this, despite the copy constructor above.
    BackupJob & operator=(const BackupJob & rhs);

    struct DbInfo {
        const string checksum;
        const string type;
        const string location;
        const float size;
    };

    const BackupInfo backup_info;
    ResilientSenderPtr sender;
    const CommandList commands;
    const int segment_max_size;
    const string swift_container;
    const string tenant;
    const double time_out;
    const string token;
    const int zlib_buffer_size;

    void dump() {
        // Record the filesystem stats before the backup is run
        Interrogator question;
        FileSystemStatsPtr stats = question.get_filesystem_stats("/var/lib/mysql");

        NOVA_LOG_DEBUG("Volume used: %.2f", stats->used);

        CommandList cmds;
        BackupProcessReader reader(commands, zlib_buffer_size, time_out);

        // Setup SwiftClient
        SwiftFileInfo file_info(backup_info.location, swift_container,
                                backup_info.id);
        SwiftUploader writer(token, segment_max_size, file_info);

        // Save the backup information to the database in case of failure
        // This allows delete calls to clean up after failures.
        // https://bugs.launchpad.net/trove/+bug/1246003
        DbInfo pre_info = {
                "",
                "xtrabackup_v1",
                file_info.manifest_url(),
                stats->used
        };
        update_db("BUILDING", pre_info);

        // Write the backup to swift.
        // The checksum returned is the swift checksum of the concatenated
        // segment checksums.
        auto checksum = writer.write(reader);

        // check the process was successful
        if (!reader.successful()) {
            update_db("FAILED");
            NOVA_LOG_ERROR("Reader was unsuccessful! Setting backup to FAILED!");
        } else {
            // stats.used
            DbInfo info = {
                checksum,
                "xtrabackup_v1",
                file_info.manifest_url(),
                stats->used
            };
            update_db("COMPLETED", info);
        }
    }

    void update_db(const string & state,
                   const optional<const DbInfo> & extra_info = boost::none) {
        IsoDateTime iso_now;

        std::string sent = str(format("%8.8f") % now());

        if(extra_info) {
            sender->send("update_backup",
                "backup_id", backup_info.id,
                "backup_type", extra_info->type,
                "checksum", extra_info->checksum,
                "location", extra_info->location,
                "size", extra_info->size,
                "state", state,
                "updated", iso_now.c_str());
        } else {
            sender->send("update_backup",
                "backup_id", backup_info.id,
                "state", state,
                "updated", iso_now.c_str());
        }

        NOVA_LOG_INFO("Updating backup %s to state %s", backup_info.id.c_str(),
                       state.c_str());
    }
};


}  // End anonymous namespace


/**---------------------------------------------------------------------------
 *- BackupManager
 *---------------------------------------------------------------------------*/

BackupManager::BackupManager(
    ResilientSenderPtr sender,
    JobRunner & runner,
    const CommandList commands,
    const int segment_max_size,
    const string swift_container,
    const double time_out,
    const int zlib_buffer_size)
:   sender(sender),
    commands(commands),
    runner(runner),
    segment_max_size(segment_max_size),
    swift_container(swift_container),
    time_out(time_out),
    zlib_buffer_size(zlib_buffer_size)
{
}

BackupManager::~BackupManager() {
}

void BackupManager::run_backup(const string & tenant,
                               const string & token,
                               const BackupInfo & backup_info) {
    NOVA_LOG_INFO("Starting backup for tenant %s, backup_id=%d",
                   tenant.c_str(), backup_info.id.c_str());
    #ifdef _DEBUG
        NOVA_LOG_INFO("Token = %s", token.c_str());
    #endif

    BackupJob job(sender, commands, segment_max_size,
                  swift_container, time_out, tenant, token,
                  zlib_buffer_size, backup_info);
    runner.run(job);
}


} } } // end namespace nova::guest::backup
