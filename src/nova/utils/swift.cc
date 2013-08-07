#include "pch.hpp"
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


struct SwiftUploader::SegmentInfo {
    size_t bytes_read;
    Md5 checksum; // segment checksum
    Md5 & file_checksum; // total file checksum
    SwiftUploader::Input & input;
    SwiftUploader & writer;

    SegmentInfo(SwiftUploader & writer, SwiftUploader::Input & input,
                Md5 & file_checksum)
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
        this->checksum.update(buffer, bytes_read);
        this->file_checksum.update(buffer, bytes_read);
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
    return str(format("%s/%s/%s")
                      % base_url % container % base_file_name);
}

string SwiftFileInfo::segment_header(int file_number) const {
    return str(format("X-Object-Meta-Segments: %d") % file_number);
}

string SwiftFileInfo::prefix_header() const {
    return str(format("X-Object-Manifest: %s/%s_")
                         % container % base_file_name);
}

string SwiftFileInfo::file_checksum_header(const string & final_file_checksum) const {
    return str(format("X-Object-Meta-File-Checksum: %s") % final_file_checksum);
}

/**---------------------------------------------------------------------------
 *- SwiftClient
 *---------------------------------------------------------------------------*/

SwiftClient::SwiftClient(const string & token)
:   session(),
    token(token)
{
}

SwiftClient::~SwiftClient()
{
}

void SwiftClient::add_token() {
    const auto header = str(format("X-Auth-Token: %s") % token);
    session.add_header(header.c_str());
}

void SwiftClient::reset_session() {
    session.reset();
    add_token();
}



/**---------------------------------------------------------------------------
 *- SwiftDownloader::Output
 *---------------------------------------------------------------------------*/

SwiftDownloader::Output::~Output() {
}

/**---------------------------------------------------------------------------
 *- SwiftDownloader
 *---------------------------------------------------------------------------*/

SwiftDownloader::SwiftDownloader(const string & token, const std::string & url)
:   SwiftClient(token),
    url(url)
{
}

SwiftDownloader::SwiftDownloader(const string & token,
                                 const SwiftFileInfo & file_info)
:   SwiftClient(token),
    url(file_info.manifest_url())
{
}

void SwiftDownloader::read(SwiftDownloader::Output & output) {
    reset_session();

    NOVA_SWIFT_LOG(format("Reading segment..."));
    session.set_opt(CURLOPT_URL, url.c_str());

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
}


/**---------------------------------------------------------------------------
 *- SwiftUploader:Input
 *---------------------------------------------------------------------------*/

SwiftUploader::Input::~Input() {
}


/**---------------------------------------------------------------------------
 *- SwiftUploader
 *---------------------------------------------------------------------------*/

SwiftUploader::SwiftUploader(const string & token,
                             const size_t & max_bytes,
                             const SwiftFileInfo & file_info)
:   SwiftClient(token),
    file_checksum(),
    swift_checksum(),
    file_info(file_info),
    file_number(0),
    max_bytes(max_bytes)
{
}


void SwiftUploader::write_manifest(int file_number,
                                   const string & final_file_checksum,
                                   const string & concatenated_checksum)
{
    reset_session();
    session.add_header(file_info.prefix_header().c_str());
    session.add_header(file_info.segment_header(file_number).c_str());
    session.add_header(file_info.file_checksum_header(final_file_checksum).c_str());
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

    Curl::HeadersPtr headers = session.head(file_info.manifest_url(), list_of(200)(202));
    // So it looks like HEAD of the manifest file returns an etag that is double quoted
    // e.g. 'etag': '"c4bf3693422e0e5a3350dac64e002987"'
    // hence we start substr at position 1
    string etag = (*headers)["etag"].substr(1, 32);
    // string etag = (*headers)["etag"].c_str();
    NOVA_LOG_DEBUG("Manifest etag: %s", etag);

    if (concatenated_checksum != etag) {
        NOVA_LOG_ERROR("Checksum match failed for checksum of concatenated segment "
                       "checksums. Expected: %s, Actual: %s.",
                       concatenated_checksum, etag);
        throw SwiftException(SwiftException::SWIFT_CHECKSUM_OF_SEGMENT_CHECKSUMS_MATCH_FAIL);
    }

}

void SwiftUploader::write_container(){
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

string SwiftUploader::write_segment(const string & url,
                                    SwiftUploader::Input & input) {
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
    const string checksum = info.checksum.finalize();
    string etag = (*headers)["etag"].substr(0, 32);
    NOVA_LOG_DEBUG("Response etag: %s", etag);
    NOVA_LOG_DEBUG("Our calculated checksum: %s", checksum);
    if (checksum != etag) {
        NOVA_LOG_ERROR("Checksum match failed on segment. Expected %s, "
                        "actual %s.", checksum.c_str(), etag.c_str());
        throw SwiftException(SwiftException::SWIFT_SEGMENT_CHECKSUM_MATCH_FAIL);
    }
    return checksum;

}
string SwiftUploader::write(SwiftUploader::Input & input){
    NOVA_LOG_DEBUG("Writing to Swift!");
    write_container();
    while (!input.eof()) {
        file_number += 1;
        const string url = file_info.formatted_url(file_number);
        NOVA_LOG_DEBUG("Time to write segment %d.", file_number);
        string md5 = write_segment(url, input);
        NOVA_LOG_DEBUG("checksum: %s", md5);
        // The swift checksum is the checksum of the concatenated segment checksums
        // Instead of holding a vector of segment checksums and then joining them
        // to calculate the checksum at the end, lets just continuously calculate
        // the checksum with md5.update to keep a minimal memory footprint.
        // Currently we have no use for all the segment checksums other than for this.
        swift_checksum.update(md5.c_str(), md5.size());
    }

    NOVA_LOG_DEBUG("Finalizing files...");
    const string final_file_checksum = file_checksum.finalize();
    NOVA_LOG_DEBUG("Checksum for entire file: %s", final_file_checksum);
    const string final_swift_checksum = swift_checksum.finalize();
    NOVA_LOG_DEBUG("Checksum of concatenated segment checksums: %s",
                   final_swift_checksum);

    write_manifest(file_number, final_file_checksum, final_swift_checksum);
    return final_swift_checksum;
}


/**---------------------------------------------------------------------------
 *- SwiftException
 *---------------------------------------------------------------------------*/

SwiftException::SwiftException(Code code) throw()
:   code(code) {
}

SwiftException::~SwiftException() throw() {
}

const char * SwiftException::what() const throw() {
    switch(code) {
        case SWIFT_SEGMENT_CHECKSUM_MATCH_FAIL:
            return "Failure matching segment checksum of swift upload!";
        case SWIFT_CHECKSUM_OF_SEGMENT_CHECKSUMS_MATCH_FAIL:
            return "Failure matching checksum of concatenated segment checksums of swift upload!";
        default:
            return "A Swift Error occurred!";
    }
}


} } }  // end namespace nova::guest::backup
