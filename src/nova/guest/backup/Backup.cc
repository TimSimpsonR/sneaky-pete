#include "nova/guest/backup.h"

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

using namespace boost::assign;

using namespace nova::db::mysql;
using nova::Process;
using nova::ProcessException;
using std::string;
using std::stringstream;
using namespace boost;
using namespace std;
using nova::utils::Curl;
using nova::utils::CurlScope;
using nova::guest::utils::IsoDateTime;
using nova::utils::Job;
using nova::utils::JobRunner;
using nova::db::mysql::MySqlConnectionWithDefaultDbPtr;
using nova::utils::swift::SwiftClient;
using nova::utils::swift::SwiftFileInfo;
using namespace nova::guest::diagnostics;

namespace nova { namespace guest { namespace backup {

namespace {

    class BackupProcessReader : public SwiftClient::Input {
    public:

        BackupProcessReader(Process::CommandList cmds, optional<double> time_out)
        :   process(cmds, false),
            time_out(time_out)
        {
        }

        virtual ~BackupProcessReader() {
        }

        virtual bool successful() const {
            return process.successful();
        }

        virtual bool eof() const {
            return process.eof();
        }

        virtual size_t read(char * buffer, size_t bytes) {
            return process.read_into(buffer, bytes, time_out);
        }

    private:
        Process process;
        optional<double> time_out;
    };

}

/**---------------------------------------------------------------------------
 *- BackupManager
 *---------------------------------------------------------------------------*/

BackupManager::BackupManager(
    MySqlConnectionWithDefaultDbPtr & infra_db,
    JobRunner & runner,
    const int chunk_size,
    const int segment_max_size,
    const std::string swift_container,
    const bool use_gzip,
    const double time_out)
:   infra_db(infra_db),
    chunk_size(chunk_size),
    runner(runner),
    segment_max_size(segment_max_size),
    swift_container(swift_container),
    use_gzip(use_gzip),
    time_out(time_out)
{
}

BackupManager::~BackupManager() {
}

void BackupManager::run_backup(const std::string & swift_url,
                        const std::string & tenant,
                        const std::string & token,
                        const std::string & backup_id) {
    NOVA_LOG_INFO2("Starting backup for tenant %s, backup_id=%d",
                   tenant.c_str(), backup_id.c_str());
    #ifdef _DEBUG
        NOVA_LOG_INFO2("Token = %s", token.c_str());
    #endif

    BackupJob job(infra_db, chunk_size, segment_max_size, swift_container,
                  swift_url, time_out, tenant, token, backup_id);
    runner.run(job);
}


/**---------------------------------------------------------------------------
 *- BackupJob
 *---------------------------------------------------------------------------*/


BackupJob::BackupJob(MySqlConnectionWithDefaultDbPtr infra_db,
                           const int & chunk_size,
                           const int & segment_max_size,
                           const std::string & swift_container,
                           const std::string & swift_url,
                           const double time_out,
                           const std::string & tenant,
                           const std::string & token,
                           const std::string & backup_id)
:   backup_id(backup_id),
    infra_db(infra_db),
    chunk_size(chunk_size),
    segment_max_size(segment_max_size),
    swift_container(swift_container),
    swift_url(swift_url),
    tenant(tenant),
    time_out(time_out),
    token(token)
{
}

BackupJob::BackupJob(const BackupJob & other)
:   backup_id(other.backup_id),
    infra_db(other.infra_db),
    chunk_size(other.chunk_size),
    segment_max_size(other.segment_max_size),
    swift_container(other.swift_container),
    swift_url(other.swift_url),
    tenant(other.tenant),
    time_out(other.time_out),
    token(other.token)
{
}

BackupJob::~BackupJob() {
}

void BackupJob::operator()() {
    auto state = get_state();
    if (!state || "NEW" != state.get()) {
        NOVA_LOG_ERROR2("State was not NEW, but %s!",
                        state.get_value_or("<not found>").c_str());
        throw BackupException(BackupException::INVALID_STATE);
    }

    update_db("BUILDING");
    NOVA_LOG_INFO("Starting backup...");
    try {
        // Start process
        // As we read, write to Swift
        dump();
        NOVA_LOG_INFO("Backup comleted successfully!");
    } catch(const std::exception & ex) {
        NOVA_LOG_ERROR("Error running backup!");
        NOVA_LOG_ERROR2("Exception: %s", ex.what());
        update_db("FAILED");
    } catch(...) {
        NOVA_LOG_ERROR("Error running backup!");
        update_db("FAILED");
    }
}

Job * BackupJob::clone() const {
    return new BackupJob(*this);
}

void BackupJob::dump() {
    // Record the filesystem stats before the backup is run
    Interrogator question;
    FileSystemStatsPtr stats = question.get_filesystem_stats("/var/lib/mysql");

    NOVA_LOG_DEBUG2("Volume used: %d", stats->used);

    Process::CommandList cmds;
    /* BACKUP COMMAND TO RUN
     * TODO: (rmyers) Make this a config option
     * 'sudo innobackupex'\
     * ' --stream=xbstream'\
     * ' /var/lib/mysql 2>/tmp/innobackupex.log | gzip'
     */
    // cmds += "/usr/bin/sudo", "-E", "innobackupex";
    // cmds += "--stream=xbstream", "/var/lib/mysql";
    // cmds += "2>/tmp/innobackupex.log", "|", "gzip";
    //cmds += "/usr/bin/sudo", "-E", "innobackupex";
    //cmds += "--stream=xbstream", "/var/lib/mysql";
    cmds += "/usr/bin/sudo", "-E", "/var/lib/nova/backup";
    // TODO: (rmyers) compress and record errors!
    //cmds += "2>/tmp/innobackupex.log";
    BackupProcessReader reader(cmds, time_out);

    // Setup SwiftClient
    SwiftFileInfo file_info(swift_url, swift_container, backup_id);
    SwiftClient writer(token, segment_max_size, chunk_size, file_info);

    // Write the backup to swift
    auto checksum = writer.write(reader);

    // check the process was successful
    if (!reader.successful()) {
        update_db("FAILED");
        NOVA_LOG_ERROR("Reader was unsuccessful! Setting backup to FAILED!");
    } else {
        // TODO: (rmyers) Add in the file space used
        // stats.used
        DbInfo info = { checksum, "xtrabackup_v1", file_info.manifest_url() };
        update_db("COMPLETED", info);
    }
}

optional<string> BackupJob::get_state() {
    auto query = str(format("SELECT state FROM backups WHERE id='%s' "
                            "AND tenant_id='%s';") % backup_id % tenant);
    MySqlResultSetPtr result = infra_db->query(query.c_str());
    if (result->next()) {
        auto state = result->get_string(0);
        if (state) {
            return state.get();
        }
    }
    return boost::none;
}

void BackupJob::update_db(const string & state,
                          const optional<const DbInfo> & extra_info) {
    const char * text =
        (!extra_info) ? "UPDATE backups SET updated=?, state=? "
                        "WHERE id=? AND tenant_id=?"
                      : "UPDATE backups SET updated=?, state=?, "
                        "checksum=NULL, backup_type=?, location=? "
                        "WHERE id=? AND tenant_id=?";
    auto stmt = infra_db->prepare_statement(text);
    IsoDateTime now;
    unsigned int index = 0;
    stmt->set_string(index ++, now.c_str());
    stmt->set_string(index ++, state.c_str());
    if (extra_info) {
        //TODO(tim.simpson): Update the checksum once we actually have one.
        //stmt->set_string(index ++, extra_info->checksum.c_str());
        stmt->set_string(index ++, extra_info->type.c_str());
        stmt->set_string(index ++, extra_info->location.c_str());
    }
    stmt->set_string(index ++, backup_id.c_str());
    stmt->set_string(index ++, tenant.c_str());
    stmt->execute(0);
    NOVA_LOG_INFO2("Updating backup %s to state %s", backup_id.c_str(),
                   state.c_str());
}


} } } // end namespace nova::guest::backup
