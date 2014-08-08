#ifndef CONNECTION_H
#define CONNECTION_H

#include <boost/optional.hpp>
#include <string>

namespace nova { namespace redis {


class Socket {
public:
    Socket(const std::string & host, const int port,
           const int max_retries);

    virtual ~Socket();

    Socket(const Socket & other);

    Socket & operator = (const Socket & rhs);

    virtual void close();

    virtual std::string get_response(int read_bytes);

    virtual boost::optional<int> send_message(std::string message);

private:

    void connect();

    void ensure_connected();

    const std::string host;

    const int max_retries;

    const int port;

    int sockfd;
};

}}//nova::redis
#endif /* CONNECTION_H */
