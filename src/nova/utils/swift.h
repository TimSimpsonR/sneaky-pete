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
#include "nova/utils/md5.h"
#include "nova/Log.h"
#include <boost/utility.hpp>


namespace nova { namespace utils { namespace swift {


struct SwiftFileInfo {

    SwiftFileInfo();

    SwiftFileInfo(const std::string & base_url,
                  const std::string & container, const std::string & base_file);

    std::string base_file_name;

    std::string base_url;  // swift url?

    std::string container;

    std::string container_url() const;

    /* File prefix. */
    std::string formatted_url(int file_number) const;

    /* The manfiest file. This is the file which gets created last and links
     * all the segments together. */
    std::string manifest_url() const;

    std::string prefix_header() const;
};


class SwiftClient : boost::noncopyable {

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

    class Output : boost::noncopyable  {
    public:
        virtual ~Output();

        virtual void write(const char * buffer, size_t buffer_size) = 0;
    };

    SwiftClient(const std::string & token,
                const size_t & max_bytes,
                const int & chunk_size,
                const SwiftFileInfo & file_info);

    const size_t max_bytes;
    void read(Output & writer);
    std::string write(Input & reader);

private:
    struct SegmentInfo;

    int chunk_size;
    Md5 file_checksum;
    SwiftFileInfo file_info;
    int file_number;
    nova::utils::Curl session;
    const std::string token;

    void add_token();

    void reset_session();
    bool validate_segment(const std::string & url, const std::string checksum);
    void write_container();
    void write_manifest();

    /* Returns a MD5 checksum. */
    std::string write_segment(const std::string & url, Input & input);
};


} } }  // end namespace nova::guest::backup

#endif
