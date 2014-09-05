#include "pch.hpp"
#include "RedisBackup.h"
#include <boost/assign/list_of.hpp>
#include "nova/Log.h"
#include "nova/utils/ls.h"
#include "nova/redis/RedisClient.h"
#include "nova/redis/RedisException.h"
#include "nova/process.h"
#include <sstream>
#include <boost/thread.hpp>
#include <time.h>
#include <vector>

using namespace boost::assign;
using nova::backup::BackupCreationArgs;
using nova::backup::BackupManagerInfo;
using nova::backup::BackupRunnerData;
using nova::backup::BackupRestoreInfo;
using nova::process::CommandList;
using nova::process::execute;
using nova::utils::ls;
using nova::process::IndependentStdErrAndStdOut;
using nova::process::Process;
using nova::process::StdOutOnly;
using nova::process::StdIn;
using boost::format;
using boost::optional;
using std::string;
using std::stringstream;
using namespace nova::utils::swift;
using std::vector;

#define SHELL(cmd) { \
    NOVA_LOG_INFO(cmd); \
    nova::process::shell(cmd, false); \
}

#define SHELL2(msg, arg) { \
    const string cmd = str(format(msg) % arg); \
    NOVA_LOG_INFO(cmd.c_str()); \
    nova::process::shell(cmd.c_str(), false); \
}


namespace nova { namespace redis {

namespace {
    const char * const MANDATED_BACKUP_DIR="/var/lib/redis";
    const char * const MANDATED_RDB_LOCATION="/var/lib/redis/dump.rdb";
    const char * const MANDATED_AOF_LOCATION="/var/lib/redis/appendonly.aof";

    vector<string> list_var_lib_redis_files() {
        vector<string> files;
        ls(MANDATED_BACKUP_DIR, files);
        return files;
    }

    void ensure_file_appears(const char * const filepath) {
        bool file_exists = false;
        int retries = 10;
        while(!file_exists) {
            try {
                SHELL2("ls %s", filepath);
                file_exists = true;
            } catch(const std::exception & e) {
                NOVA_LOG_ERROR("%s does not exist yet!", filepath);
                const boost::posix_time::seconds one_second(1);
                boost::this_thread::sleep(one_second);
                -- retries;
                if (0 >= retries) {
                    NOVA_LOG_ERROR("Can't find file %s!", filepath);
                    throw RedisException(RedisException::BACKUP_FILE_MISSING);
                }
            }
        }
    }

}  // end anonymous namespace


string find_aof_file(RedisClient & client) {
    return client.info().get_optional("appendfilename")
        .get_value_or(MANDATED_AOF_LOCATION);
}

string find_rdb_file(RedisClient & client) {
    return client.info().get_optional("dbfilename")
        .get_value_or(MANDATED_RDB_LOCATION);
}

/**---------------------------------------------------------------------------
 *- RdbPersistence
 *---------------------------------------------------------------------------*/

RdbPersistence::RdbPersistence(long long tolerance)
:   client(),
    tolerance(tolerance) {
}


bool RdbPersistence::bgsave_or_aof_rewrite_ocurring() {
    auto info = client.info();
    return (info.get("rdb_bgsave_in_progress") != "0"
            || info.get("aof_rewrite_in_progress") != "0");
}

void RdbPersistence::save() {
    // TODO(tim.simpson): Check memory, go with BGSAVE if there's enough memory
    // call save since we're a slave
    client.command("BGSAVE").expect_status();
}

void RdbPersistence::update() {
    NOVA_LOG_INFO("Performing backup.");

    const boost::posix_time::seconds one_second(1);
    bool wait_count = 0;
    while (bgsave_or_aof_rewrite_ocurring()) {
        if (0 == wait_count % 60) {
            NOVA_LOG_INFO("aof rewrite or bgsave in progress, waiting...");
        } else {
            NOVA_LOG_TRACE("aof rewrite or bgsave in progress, waiting...");
        }
        boost::this_thread::sleep(one_second);
        ++ wait_count;
    }

    save();

    //TODO(tim.simpson): Have some kind of time out here for the bgsave.
    while(wait_for_save()) {
        NOVA_LOG_TRACE("Still saving...");
        boost::this_thread::sleep(one_second);
    }


    // Check to see that LASTSAVE is very recent.
    // save_again_ig
    // upload_to_swift();
}

bool RdbPersistence::wait_for_save() {
    //TODO: Move
    const auto last_save = client.command("LASTSAVE").expect_int();
    const auto now = ::time(NULL);

    NOVA_LOG_INFO("LASTSAVE? %s", last_save);
    NOVA_LOG_INFO("NOW?      %s", now);

    long long diff = now - last_save;
    NOVA_LOG_INFO("   diff = %s", diff);
    return (diff > tolerance);
}


/**---------------------------------------------------------------------------
 *- RedisBackupJob
 *---------------------------------------------------------------------------*/

class TarInputProcess : public SwiftUploader::Input {
public:
    TarInputProcess(CommandList cmds)
    :   error_out("/tmp/backup-errors.txt"),
        process(cmds)
    {
    }

    virtual ~TarInputProcess() {
    }

    virtual bool eof() const {
        const auto result = process.is_finished();
        if (result) {
            if (!process.successful()) {
                NOVA_LOG_ERROR("Process had a bad exit code!");
                throw RedisException(RedisException::PROCESS_FAILED);
            }
        }
        return result;
    }

    virtual size_t read(char * buffer, size_t bytes) {
        while(!process.is_finished()) {
            const auto result = process.read_into(buffer, bytes, boost::none);
            if (result.err()) {
                error_out.write(buffer, bytes);
            } else if (result.out()) {
                return result.write_length;
            } else if (result.time_out()) {
                NOVA_LOG_ERROR("Time out reading from process. Trying again...");
            }
        }
        NOVA_LOG_INFO("End of proc.");
        error_out.close();
        return 0;
    }
private:
    std::ofstream error_out;
    Process<IndependentStdErrAndStdOut> process;
};

class TarOutputProcess : public SwiftDownloader::Output {
public:
    TarOutputProcess(CommandList cmds)
    :   process(cmds) {
    }

    void write(const char * buffer, size_t buffer_size) {
        process.write(buffer, buffer_size);
    }
private:
    Process<StdIn> process;
};

RedisBackupJob::RedisBackupJob(BackupRunnerData data,
                               bool allow_master_to_backup,
                               long long tolerance,
                               const BackupCreationArgs & args)
:   BackupJob(data, args),
    allow_master_to_backup(allow_master_to_backup),
    client(),
    tolerance(tolerance) {
}

RedisBackupJob::RedisBackupJob(const RedisBackupJob & other)
:   BackupJob(other.data, other.args),
    allow_master_to_backup(other.allow_master_to_backup),
    client(),
    tolerance(other.tolerance) {
}

RedisBackupJob::~RedisBackupJob() {
}

void RedisBackupJob::operator()() {
    try {
        update_trove_to_building();
        update_trove_to_completed(run());
        NOVA_LOG_INFO("Backup complete!");
    } catch(const std::exception ex) {
        NOVA_LOG_ERROR("Error creating backup! %s", ex.what());
        update_trove_to_failed();
        throw;
    } catch(...) {
        NOVA_LOG_ERROR("Error creating backup!");
        update_trove_to_failed();
        throw;
    }
}

nova::utils::Job * RedisBackupJob::clone() const {
    return new RedisBackupJob(*this);
}

vector<string> RedisBackupJob::determine_files_to_backup() {
    // Even once Redis says its done, we may need to wait for the files to
    // become available in the directory listing. Calling fsync seems to solve
    // this entirely, but due to paranoia from edge cases we still check
    // for the individual aof and / or rdb files as well and even sleep a bit
    // before proceeding.
    NOVA_LOG_INFO("fsync'ing directory %s...", MANDATED_BACKUP_DIR);
    {
        struct File {
            int handle;
            File() {
                handle = open(MANDATED_BACKUP_DIR, O_RDONLY);
            }
            ~File() {
                close(handle);
            }
        } file;
        if (fsync(file.handle) < 0) {
            NOVA_LOG_ERROR("Error running fsync.");
            throw RedisException(RedisException::COULD_NOT_FSYNC);
        }
    }
    NOVA_LOG_INFO("Paranoia- Giving Redis time to finish writing stuff.");
    boost::this_thread::sleep(boost::posix_time::seconds(3));


    auto info = client.info();
    const bool aof = info.get("aof_enabled") != "0";
    NOVA_LOG_INFO("AOF enabled=%s", aof ? "true" : "false");

    // If rdb is disabled this returns an empty string, or so I was told.
    bool rdb;
    try {
        rdb = client.command("CONFIG GET save").expect_string() != "";
    } catch(...) {
        rdb = true;
    }
    NOVA_LOG_INFO("RDB enabled=%s", rdb ? "true" : "false");

    if (!aof && !rdb) {
        NOVA_LOG_ERROR("Can't create backup- no backup strategy is in use!");
        throw RedisException(RedisException::NO_BACKUP_STRATEGY_IN_USE);
    }

    vector<string> files = list_var_lib_redis_files();
    BOOST_FOREACH(string & file, files) {
        file = str(boost::format("%s/%s") % MANDATED_BACKUP_DIR % file);
    }
    if (aof) {
        ensure_file_exists_in(files, MANDATED_AOF_LOCATION);
    }
    if (rdb) {
        ensure_file_exists_in(files, MANDATED_RDB_LOCATION);
    }
    return files;
}

void RedisBackupJob::ensure_file_exists_in(vector<string> files,
                                           const char * const filepath) {
    if (files.end() !=
        std::find(files.begin(), files.end(), filepath)) {
        return;  // File found, so there's nothing to do.
    }
    files.push_back(filepath);
    ensure_file_appears(filepath);
}

const char * RedisBackupJob::get_backup_type() const {
    return "rdb";
}

bool RedisBackupJob::is_slave() {
    const string role = client.info().get("role");
    return role == "slave";
}

string RedisBackupJob::run() {
    if (!is_slave() && !allow_master_to_backup) {
        NOVA_LOG_ERROR("We're not a slave, so avoid the backup.");
        throw RedisException(RedisException::INVALID_BACKUP_TARGET);
    }
    if (find_rdb_file(client) != MANDATED_RDB_LOCATION) {
        NOVA_LOG_ERROR("The RDB file has been moved. The current "
            "implementation is too stupid to work with such outrageous "
            "customizations.");
        throw RedisException(RedisException::INVALID_BACKUP_TARGET);
    }
    RdbPersistence rdb(tolerance);
    rdb.update();

    auto files = determine_files_to_backup();

    return upload(files);
}


string RedisBackupJob::upload(vector<string> files) {
    NOVA_LOG_INFO("Uploading to Swift.");
    SwiftUploader writer(args.token, data.segment_max_size, file_info,
                         data.checksum_wait_time);
    NOVA_LOG_INFO("Uploading tar stream to Swift.");
    //tar zcf - /var/lib/redis/* /etc/redis/redis.conf
    CommandList cmds = list_of("/usr/bin/sudo")("/bin/tar")("zcf")("-")
        ("/etc/redis/redis.conf");
    BOOST_FOREACH(const auto & file, files) {
        cmds.push_back(file);
    }
    TarInputProcess process(cmds);
    return writer.write(process);
}



/**---------------------------------------------------------------------------
 *- RedisBackupManager
 *---------------------------------------------------------------------------*/

RedisBackupManager::RedisBackupManager(const BackupManagerInfo & info,
                                       const bool allow_master_to_backup,
                                       const long long tolerance)
:   BackupManager(info),
    allow_master_to_backup(allow_master_to_backup),
    tolerance(tolerance)
{

}


RedisBackupManager::~RedisBackupManager() {

}

void RedisBackupManager::run_backup(const BackupCreationArgs & args) {
    NOVA_LOG_INFO("Starting backup for tenant %s, backup_id=%d",
                   args.tenant.c_str(), args.id.c_str());
    RedisBackupJob job(data, allow_master_to_backup, tolerance, args);
    runner.run(job);
}


/**---------------------------------------------------------------------------
 *- RedisBackupRestoreManager
 *---------------------------------------------------------------------------*/


RedisBackupRestoreManager::RedisBackupRestoreManager() {
}

void RedisBackupRestoreManager::run(const BackupRestoreInfo & restore) {
    //cat /tmp/bk | tar zxf -
    SHELL("sudo mkdir -p /tmp");
    SHELL("sudo mkdir -p /tmp/restore");

    {
        SwiftDownloader downloader(restore.get_token(),
                                   restore.get_backup_url(),
                                   restore.get_backup_checksum());
        CommandList cmds = list_of("/usr/bin/sudo")
            ("/bin/tar")("zxf")("-")("--directory")("/tmp/restore");
        TarOutputProcess process(cmds);
        downloader.read(process);
    }

    SHELL("sudo cp -f /tmp/restore/etc/redis/redis.conf /etc/redis/redis.conf");
    SHELL2("sudo cp -f /tmp/restore/var/lib/redis/* %s/", MANDATED_BACKUP_DIR);
    SHELL("sudo chown redis:redis /etc/redis/redis.conf");
    SHELL2("sudo chown -R redis:redis %s", MANDATED_BACKUP_DIR);
    SHELL("sudo rm -rf /tmp/restore");
}

} } // end namespace redis
