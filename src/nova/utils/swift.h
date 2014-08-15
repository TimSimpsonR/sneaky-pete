#ifndef __NOVA_UTILS_SWIFT_H
#define __NOVA_UTILS_SWIFT_H

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
#include <boost/utility.hpp>
#include <exception>


namespace nova { namespace utils { namespace swift {


/* Used to determine strings necessary to store large files in Swift.
   We might need to write multiple segments along with a manifest. Given
   the root Swift URL, the container, and base_file, this class will determine
   other useful things for us. */
struct SwiftFileInfo {

    SwiftFileInfo();

    SwiftFileInfo(const std::string & base_url,
                  const std::string & container, const std::string & base_file);

    std::string base_file_name;

    std::string base_url;  // swift url?

    std::string container;

    /* base_url / container */
    std::string container_url() const;

    /* Returns a URL for a file. The argument is the index of the segment
     * being uploaded. */
    std::string formatted_url(int file_number) const;

    /* The manfiest file. This is the file which gets created last and links
     * all the segments together. */
    std::string manifest_url() const;

    /* Header necessary to write the manifest. */
    std::string prefix_header() const;

    /* Write the number of segments to the metadata */
    std::string segment_header(int file_number) const;

    /* Write the actual checksum of file to the metadata. */
    std::string file_checksum_header(const std::string & final_file_checksum) const;
};


class SwiftException : public std::exception {

    public:
        enum Code {
            SWIFT_UPLOAD_SEGMENT_CHECKSUM_MATCH_FAIL,
            SWIFT_UPLOAD_CHECKSUM_OF_SEGMENT_CHECKSUMS_MATCH_FAIL,
            SWIFT_DOWNLOAD_CHECKSUM_MATCH_FAIL
        };

        SwiftException(Code code) throw();

        virtual ~SwiftException() throw();

        virtual const char * what() const throw();

    private:
        Code code;
};


class SwiftClient : boost::noncopyable {
public:
    SwiftClient(const std::string & token);

    virtual ~SwiftClient();

protected:
    nova::utils::Curl session;

    void add_token();

    void reset_session();

private:
    const std::string token;
};


class SwiftDownloader : public SwiftClient {
public:
    class Output : boost::noncopyable  {
    public:
        virtual ~Output();

        virtual void write(const char * buffer, size_t buffer_size) = 0;
    };

    SwiftDownloader(const std::string & token,
                    const std::string & url,
                    const std::string & checksum);

    SwiftDownloader(const std::string & token,
                    const SwiftFileInfo & file_info,
                    const std::string & checksum);

    void read(Output & writer);

private:
    std::string url;
    std::string checksum;
};


class SwiftUploader : public SwiftClient {

public:
    /**
        Interface used to read from a stream of some kind.
     */
    class Input : boost::noncopyable {
    public:
        virtual ~Input();

        virtual bool eof() const = 0;

        virtual size_t read(char * buffer, size_t buffer_size) = 0;
    };


    SwiftUploader(const std::string & token,
                  const size_t & max_bytes,
                  const SwiftFileInfo & file_info,
                  const int checksum_wait_time);

    std::string write(Input & reader);

private:
    struct SegmentInfo;

    const int checksum_wait_time;
    Md5 file_checksum;
    Md5 swift_checksum;
    SwiftFileInfo file_info;
    int file_number;
    const size_t max_bytes;

    std::string await_etag_match(const std::string & url,
                                 const std::string & checksum,
                                 const char * error_text,
                                 SwiftException::Code exception_code,
                                 const bool etag_has_double_quotes);
    void write_container();
    void write_manifest(int file_number,
                        const std::string & final_file_checksum,
                        const std::string & concatenated_checksum);


    /* Returns a MD5 checksum. */
    std::string write_segment(const std::string & url, Input & input);
};


class LocalFileReader : public SwiftUploader::Input {
public:

    LocalFileReader(const char * file_name);

    virtual ~LocalFileReader();

    virtual bool eof() const;

    virtual size_t read(char * buffer, size_t bytes);

private:
    bool _eof;
    std::ifstream file;
};


class LocalFileWriter : public SwiftDownloader::Output {
public:

    LocalFileWriter(const char * file_name);

    virtual ~LocalFileWriter();

    virtual void write(const char * buffer, size_t buffer_size);

private:
    std::ofstream file;
};


} } }  // end namespace nova::guest::backup

#endif
