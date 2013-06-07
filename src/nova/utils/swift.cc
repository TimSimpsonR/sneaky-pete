#include "swift.h"
#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fstream>
#include <iostream>
#include "nova/utils/Curl.h"
#include <boost/format.hpp>
#include "nova/utils/Md5.h"
#include "nova/Log.h"
#include <boost/assign/list_of.hpp>

using namespace std;
using namespace boost;
using namespace boost::assign;
using nova::utils::Curl;
using nova::utils::CurlScope;
using nova::Log;
using nova::LogApiScope;
using nova::LogOptions;
using nova::utils::Md5;


// Define _NOVA_SWIFT_VERBOSE when building to get log messages for almost
// everything.
#ifdef _NOVA_SWIFT_VERBOSE
#define NOVA_SWIFT_LOG(msg) { std::string m = str(msg); NOVA_LOG_DEBUG(m.c_str()); }
#else
#define NOVA_SWIFT_LOG(msg) /* */
#endif

namespace nova { namespace utils { namespace swift {


struct SwiftClient::SegmentInfo {
    size_t bytes_read;
    Md5 checksum; // segment checksum
    Md5 file_checksum; // total file checksum
    SwiftClient::Input & input;
    SwiftClient & writer;

    SegmentInfo(SwiftClient & writer, SwiftClient::Input & input, Md5 & file_checksum)
    :   bytes_read(0),
        checksum(),
        file_checksum(file_checksum),
        input(input),
        writer(writer)
    {
    }

    size_t callback(char * buffer, size_t buffer_size) {
        // Read up to Max segment size
        const auto remainder = this->writer.max_bytes - this->bytes_read;
        // Don't bother with a network operation, even if we could add a bit more.
        if (buffer_size > remainder) {
            return 0;
        }
        const auto bytes_read = this->input.read(buffer, buffer_size);
        this->bytes_read += bytes_read;
        this->checksum.add(buffer, bytes_read);
        this->file_checksum.add(buffer, bytes_read);
        NOVA_SWIFT_LOG(format("Curl upload call back.\n bytes_read=%d, "
                       "total bytes_read=%d") % bytes_read % this->bytes_read);
        return bytes_read;
    }

    /* This is the C interface Curl wants us to use. */
    static size_t curl_callback(void * ptr, size_t size, size_t nmemb,
                                void * user_ptr) {
        auto * self = reinterpret_cast<SegmentInfo *>(user_ptr);
        return self->callback(reinterpret_cast<char *>(ptr), size * nmemb);
    }
};

/**---------------------------------------------------------------------------
 *- SwiftFileInfo
 *---------------------------------------------------------------------------*/
SwiftFileInfo::SwiftFileInfo()
:   base_file_name(),
    base_url(),
    container()
{
}

SwiftFileInfo::SwiftFileInfo(const string & base_url, const string & container,
                             const string & base_file)
:   base_file_name(base_file),
    base_url(base_url),
    container(container)
{
}

string SwiftFileInfo::container_url() const {
    return str(format("%s/%s") % base_url % container);
}

string SwiftFileInfo::formatted_url(int file_number) const {
    auto formatted = str(format("%s/%s_%08d") % container_url()
                         % base_file_name % file_number);
    return formatted;
}

string SwiftFileInfo::manifest_url() const {
    return str(format("%s/%s/%s.xbstream.gz")
                      % base_url % container % base_file_name);
}

string SwiftFileInfo::prefix_header() const {
    return str(format("X-Object-Manifest: %s/%s_")
                         % container % base_file_name);
}

/**---------------------------------------------------------------------------
 *- SwiftClient::Input
 *---------------------------------------------------------------------------*/

SwiftClient::Input::~Input() {
}


/**---------------------------------------------------------------------------
 *- SwiftClient::Output
 *---------------------------------------------------------------------------*/

SwiftClient::Output::~Output() {
}


/**---------------------------------------------------------------------------
 *- SwiftClient
 *---------------------------------------------------------------------------*/

SwiftClient::SwiftClient(const string & token, const size_t & max_bytes,
                         const int & chunk_size,
                         const SwiftFileInfo & file_info)
:   max_bytes(max_bytes),
    chunk_size(chunk_size), // this should be 65536 once we figure out how!
    file_checksum(),
    file_info(file_info),
    file_number(1),
    session(),
    token(token)
{
}

void SwiftClient::add_token() {
    const auto header = str(format("X-Auth-Token: %s") % token);
    session.add_header(header.c_str());
}

void SwiftClient::read(SwiftClient::Output & output) {
    reset_session();

    NOVA_SWIFT_LOG(format("Reading segment..."));
    session.set_opt(CURLOPT_URL, file_info.manifest_url().c_str());

    struct CallBack {
        static size_t curl_callback(void * ptr, size_t size, size_t nmemb,
                                    void * userdata) {
            Output * output = reinterpret_cast<Output *>(userdata);
            const char * buffer = reinterpret_cast<const char *>(ptr);
            const size_t buffer_size = size * nmemb;
            NOVA_SWIFT_LOG(format("Read callback: %d") % buffer_size);
            output->write(buffer, buffer_size);
            // Assume success. In C++ land we have exceptions.
            return buffer_size;
        }
    };

    // TODO: (rmyers) Recompile libcurl and set CURL_MAX_WRITE_SIZE = 65536
    //       Then figure out if this can be increased as well.
    session.set_opt(CURLOPT_BUFFERSIZE, 16372L);
    session.set_opt(CURLOPT_WRITEFUNCTION, CallBack::curl_callback);
    session.set_opt(CURLOPT_WRITEDATA, &output);

    /* Let's do this! */
    session.perform(list_of(200));

    // TODO(tim.simpson): Compare with checksum somehow. Although, Maybe that
    //                    should happen in the Output subclass.
}

void SwiftClient::write_manifest(){
    reset_session();
    session.add_header(file_info.prefix_header().c_str());
    /* enable uploading */
    session.set_opt(CURLOPT_UPLOAD, 1L);

    /* HTTP PUT please */
    session.set_opt(CURLOPT_PUT, 1L);

    /* specify target URL, and note that this URL should include a file
       name, not only a directory */
    session.set_opt(CURLOPT_URL, file_info.manifest_url().c_str());

    /* now specify which file to upload */
    session.set_opt(CURLOPT_INFILESIZE_LARGE, 0);

    /* Make it happen */
    session.perform(list_of(200)(201)(202));
}

void SwiftClient::write_container(){
    reset_session();
    NOVA_SWIFT_LOG(format("Writing container to %s...")
                   % file_info.container_url());
    /* enable uploading */
    session.set_opt(CURLOPT_UPLOAD, 1L);

    /* HTTP PUT please */
    session.set_opt(CURLOPT_PUT, 1L);

    /* specify target URL, and note that this URL should include a file
       name, not only a directory */
    session.set_opt(CURLOPT_URL, file_info.container_url().c_str());

    /* now specify which file to upload */
    session.set_opt(CURLOPT_INFILESIZE_LARGE, 0);

    /* Make it happen */
    session.perform(list_of(200)(201)(202));
    NOVA_SWIFT_LOG(format("Containing write complete."));
}

void SwiftClient::reset_session() {
    session.reset();
    add_token();
}

string SwiftClient::write_segment(const string & url, SwiftClient::Input & input) {
    reset_session();

    NOVA_SWIFT_LOG(format("Writing segment..."));
    session.set_opt(CURLOPT_UPLOAD, 1L);
    session.set_opt(CURLOPT_PUT, 1L);    // Use PUT
    session.set_opt(CURLOPT_URL, url.c_str());
    // TODO: (rmyers) Recompile libcurl and set CURL_MAX_WRITE_SIZE = 65536
    //       Then figure out if this can be increased as well.
    session.set_opt(CURLOPT_BUFFERSIZE, 16372L);

    /* Tell Curl to call into SegmentInfo each time. */
    SegmentInfo info(*this, input, file_checksum);
    session.set_opt(CURLOPT_READFUNCTION, SegmentInfo::curl_callback);
    session.set_opt(CURLOPT_READDATA, &info);

    /* Let's do this! */
    session.perform(list_of(201)(202));

    Curl::HeadersPtr headers = session.head(url, list_of(200)(202));
    const string checksum = info.checksum.finish();
    string etag = (*headers)["etag"].substr(0, 32);
    NOVA_LOG_ERROR2("What was etag? %s", etag.c_str());
    if (checksum != etag) {
        NOVA_LOG_ERROR2("Checksum match failed on segment. Expected %s, "
                        "actual %s.", checksum.c_str(), etag.c_str());
        throw std::exception();
    }
    return checksum;
}

bool SwiftClient::validate_segment(const string & url, const string checksum){
    // validate the segment against cloud files
    reset_session();
    session.set_opt(CURLOPT_URL, url.c_str());
    // TODO(tim.simpson): Finish this.
    return true;
}


string SwiftClient::write(SwiftClient::Input & input){
    NOVA_SWIFT_LOG(format("Writing to Swift!"));
    write_container();
    while (!input.eof()) {
        const string url = file_info.formatted_url(file_number);
        NOVA_SWIFT_LOG(format("Time to write segment %d.") % file_number);
        string md5 = write_segment(url, input);
        // Head the file to check the checksum
        NOVA_SWIFT_LOG(format("checksum: %s") % md5);
        file_number += 1;
    }
    NOVA_SWIFT_LOG(format("Finalizing files..."));
    write_manifest();
    return file_checksum.finish();
}


} } }  // end namespace nova::guest::backup
