#include "pch.hpp"
#include "MySqlBackupManager.h"
#include "nova/backup/BackupException.h"
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
using nova::backup::BackupCreationArgs;
using nova::backup::BackupJob;
using nova::backup::BackupInfo;
using nova::backup::BackupManagerInfo;
using nova::backup::BackupRunnerData;
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

namespace nova { namespace guest { namespace mysql {

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
                if (result.time_out()) {
                    NOVA_LOG_ERROR("Time out while looking for output from backup "
                                   "process. Reading again...");
                } else {
                    NOVA_LOG_INFO("Out of input.");
                }
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
 *- XtraBackupJob
 *---------------------------------------------------------------------------*/

/** Represents an instance of a in-flight backup. */
class XtraBackupJob : public BackupJob {
public:
    XtraBackupJob(
        const BackupRunnerData & data,
        const CommandList commands,
        const size_t & zlib_buffer_size,
        const BackupCreationArgs & args)
    :   BackupJob(data, args),
        commands(commands),
        zlib_buffer_size(zlib_buffer_size) {
    }

    // Copy constructor is designed mainly so we can pass instances
    // from one thread to another.
    XtraBackupJob(const XtraBackupJob & other)
    :   BackupJob(other.data, other.args),
        commands(other.commands),
        zlib_buffer_size(other.zlib_buffer_size) {
    }

    ~XtraBackupJob() {
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
            update_trove_to_failed();
        } catch(...) {
            NOVA_LOG_ERROR("Error running backup!");
            update_trove_to_failed();
        }
    }

    virtual Job * clone() const {
        return new XtraBackupJob(*this);
    }

protected:
    virtual const char * get_backup_type() const {
        return "xtrabackup_v1";
    }

private:
    // Don't allow this, despite the copy constructor above.
    XtraBackupJob & operator=(const XtraBackupJob & rhs);

    const CommandList commands;
    const int zlib_buffer_size;

    void dump() {
        CommandList cmds;
        BackupProcessReader reader(commands, zlib_buffer_size, data.time_out);

        // Setup SwiftClient
        SwiftUploader writer(args.token, data.segment_max_size, file_info,
                             data.checksum_wait_time);

        update_trove_to_building();

        // Write the backup to swift.
        // The checksum returned is the swift checksum of the concatenated
        // segment checksums.
        auto checksum = writer.write(reader);

        // check the process was successful
        if (!reader.successful()) {
            update_trove_to_failed();
            NOVA_LOG_ERROR("Reader was unsuccessful! Setting backup to FAILED!");
        } else {
            update_trove_to_completed(checksum);
        }
    }

};


}  // End anonymous namespace


/**---------------------------------------------------------------------------
 *- BackupManager
 *---------------------------------------------------------------------------*/

MySqlBackupManager::MySqlBackupManager(
    const BackupManagerInfo & info,
    const CommandList commands,
    const int zlib_buffer_size)
:   BackupManager(info),
    commands(commands),
    zlib_buffer_size(zlib_buffer_size)
{
}

MySqlBackupManager::~MySqlBackupManager() {
}

void MySqlBackupManager::run_backup(const BackupCreationArgs & args) {
    NOVA_LOG_INFO("Starting backup for tenant %s, backup_id=%d",
                   args.tenant.c_str(), args.id.c_str());
    #ifdef _DEBUG
        NOVA_LOG_INFO("Token = %s", args.token.c_str());
    #endif

    XtraBackupJob job(data, commands, zlib_buffer_size, args);
    runner.run(job);
}


} } } // end namespace nova::guest::backup
