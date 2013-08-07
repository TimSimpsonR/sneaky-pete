#include "pch.hpp"
#include "BackupRestore.h"
#include <boost/foreach.hpp>
#include "nova/utils/io.h"
#include <boost/assign/list_of.hpp>
#include <boost/assign/std/list.hpp>
#include "nova/Log.h"
#include "nova/process.h"
#include <sstream>
#include "nova/utils/swift.h"
#include <vector>
#include "nova/utils/zlib.h"

using namespace boost::assign;
using boost::format;
using namespace nova::process;
using std::string;
using std::stringstream;
using nova::utils::swift::SwiftDownloader;
using std::vector;
namespace zlib = nova::utils::zlib;

namespace nova { namespace guest { namespace backup {

namespace {

    const char * mysqldir = "/var/lib/mysql";

    class StdErrToLogFile : public StdErrToFile {
        virtual const char * log_file_name() {
            return "/var/log/nova/guest.log";
        }
    };

} // end anonymous namespace


/**---------------------------------------------------------------------------
 *- BackupRestoreInfo
 *---------------------------------------------------------------------------*/

BackupRestoreInfo::BackupRestoreInfo(const string & token, const string & backup_url)
:   backup_url(backup_url),
    token(token)
{
}


/**---------------------------------------------------------------------------
 *- BackupRestoreJob
 *---------------------------------------------------------------------------*/

class BackupRestoreManager::BackupRestoreJob {
public:
    BackupRestoreJob(const BackupRestoreManager & manager,
                     const BackupRestoreInfo & info)
    :   info(info),
        manager(manager)
    {
    }

    void execute() {
        NOVA_LOG_DEBUG("Cleaning up some files in MySQL install direcotry...");
        clean_existing_files();
        NOVA_LOG_DEBUG("Extracting backup...");
        extract_backup();
        NOVA_LOG_DEBUG("Preparing the backup with the database...");
        prepare_db();
        NOVA_LOG_DEBUG("Restore finished without signs of errors.");
    }

private:
    const BackupRestoreInfo & info;
    const BackupRestoreManager & manager;

    void clean_existing_files() {
        const string & restore_dir = manager.restore_directory;
        vector<string> directory_contents;
        ls(restore_dir, directory_contents);
        BOOST_FOREACH(const std::string & file, directory_contents) {
            if (manager.save_file_pattern.has_match(file)) {
                NOVA_LOG_DEBUG("Skipping removal of file %s.", file);
            } else if (manager.delete_file_pattern.has_match(file)) {
                rm_rf(str(boost::format("%s/%s") % restore_dir % file));
            } else {
                NOVA_LOG_ERROR("Error: unknown file: %s", file);
            }
        }
    }

    void extract_backup() {
        /* The following code replaces this bash script:
         * /usr/bin/curl -s -H "X-Auth-Token: $TOKEN" -G $URL \
         *    | /bin/gunzip - \
         *    | /usr/bin/xbstream -x -C $RESTORE \
         *    2>>$LOGFILE
         */

        /* ZLib output stream which, on every write event, pipes data to
         * the given process's stdin stream. */
        struct ZlibOutput : public zlib::OutputStream {

            ZlibOutput(Process<StdIn, StdErrToLogFile> & process)
            :   process(process)
            {
            }

            virtual zlib::ZlibBufferStatus advance() {
                return zlib::OK;
            }

            virtual char * get_buffer() {
                return output_buffer;
            }

            virtual size_t get_buffer_size() {
                return sizeof(output_buffer) - 1;
            }

            virtual zlib::ZlibBufferStatus notify_written(const size_t write_count) {
                output_buffer[write_count] = '\0';
                process.write(output_buffer, write_count);
                return zlib::OK;
            }

            char output_buffer[1024];
            Process<StdIn, StdErrToLogFile> & process;
        };

        /* A target for a SwiftDownloader, which, on getting data,
         * decompresses it to a zlib target which does other stuff. */
        struct DownloadWriter : public SwiftDownloader::Output {
            zlib::ZlibDecompressor & decompressor;
            zlib::OutputStreamPtr zlib_output;

            DownloadWriter(zlib::ZlibDecompressor & decompressor,
                           zlib::OutputStreamPtr & zlib_output)
            :   decompressor(decompressor),
                zlib_output(zlib_output)
            {
            }

            void write(const char * buffer, size_t buffer_size) {
                //TODO(tim.simpson): Make the char * buffer to zlib's input
                //                   streams constant. It doesn't write to it.
                decompressor.run_read_from(const_cast<char *>(buffer),
                                           buffer_size,
                                           zlib_output);
            }
        } ;

        // Now plug all this stuff together and hope it forms Super Ulta
        // Mega Mega Man.
        CommandList cmds = list_of("/usr/bin/sudo")("-E")
                                  ("/usr/bin/xbstream")("-x")("-C")
                                  (manager.restore_directory.c_str());
        Process<StdIn, StdErrToLogFile> xbstream_proc(cmds);

        {
            zlib::ZlibDecompressor decompressor;
            zlib::OutputStreamPtr decompressor_source(
                static_cast<zlib::OutputStream *>(
                    new ZlibOutput(xbstream_proc)));

            // Download content, unzip it to xbstream in the process.
            {
                DownloadWriter swift_output(decompressor,
                                            decompressor_source);
                SwiftDownloader swift_downloader(
                    info.get_token(),
                    info.get_backup_url());
                swift_downloader.read(swift_output);
            }

            if (!decompressor.is_finished()) {
                NOVA_LOG_ERROR("Did not get a complete, valid zip file from "
                               "swift! This may mean the original backup was "
                               "invalid or the download stream was corrupted.");
                throw BackupRestoreException();
            }
        }
        xbstream_proc.wait_forever_for_exit();
        if (!xbstream_proc.successful()) {
            NOVA_LOG_ERROR("Error running restore xbstream process!");
            throw ProcessException(ProcessException::EXIT_CODE_NOT_ZERO);
        }
        NOVA_LOG_DEBUG("Backup successfully extracted.");
    }  // end of extract_backup method.

    void ls(const string & directory, vector<string> & output) {
        stringstream stdout;
        const CommandList cmds = list_of("/usr/bin/sudo")("-E")("/bin/ls")
                                        (directory.c_str());
        nova::process::execute(stdout, cmds, 60.0);
        while(stdout.good()) {
            string item;
            stdout >> item;
            output.push_back(item);
        }
    }

    void prepare_db() {
        NOVA_LOG_DEBUG("Preparing db using innobackupex!");
        string default_file = str(format("--defaults-file=%s/backup-my.cnf")
                                  % manager.restore_directory);
        CommandList cmds = list_of("/usr/bin/sudo")("-E")
            ("/usr/bin/innobackupex")("--apply-log")(mysqldir)
            (default_file.c_str())("--ibbackup")("xtrabackup");
        Process<StdErrToLogFile> innobackupex_proc(cmds);
        innobackupex_proc.wait_forever_for_exit();
        if (!innobackupex_proc.successful()) {
            NOVA_LOG_ERROR("Error running restore innobackupex process!");
        }

        CommandList cmds2 = list_of("/usr/bin/sudo")("-E")
                                   ("/bin/chown")("-R")("mysql")(mysqldir);
        Process<> chown(cmds2);
        chown.wait_forever_for_exit();
        if (!chown.successful()) {
            NOVA_LOG_ERROR("Error running chown on prepared database!");
        }
    }

    void rm_rf(const string & path) {
        NOVA_LOG_DEBUG("rm -rf %s", path);
        CommandList cmds = list_of("/usr/bin/sudo")("-E")("rm")("-rf")
                                  (path.c_str());
        Process<> proc(cmds);
        proc.wait_forever_for_exit();
    }
};


/**---------------------------------------------------------------------------
 *- BackupRestoreManager
 *---------------------------------------------------------------------------*/

BackupRestoreManager::BackupRestoreManager(
    const int chunk_size,
    const CommandList command_list,
    const std::string & delete_file_pattern,
    const std::string & restore_directory,
    const std::string & save_file_pattern)
:   chunk_size(chunk_size),
    commands(command_list),
    delete_file_pattern(delete_file_pattern.c_str()),
    restore_directory(restore_directory),
    save_file_pattern(save_file_pattern.c_str())
{
}

void BackupRestoreManager::run(const BackupRestoreInfo & restore) {
    BackupRestoreJob job(*this, restore);
    job.execute();
}


/**----------------------------------------------   -----------------------------
 *- BackupRestoreException
 *---------------------------------------------------------------------------*/

const char * BackupRestoreException::what() const throw() {
    return "Error performing backup restore!";
}


} } } // end namespace nova::guest::backup
