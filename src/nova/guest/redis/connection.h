#ifndef CONNECTION_H
#define CONNECTION_H

#include <string>

namespace nova { namespace redis {


class Socket {
public:
    //TODO(tim.simpson): port should be an integer!
    Socket(const std::string & host, const int port,
           const int max_retries);

    ~Socket();

    Socket(const Socket & other);

    Socket & operator = (const Socket & rhs);

    void close();

    std::string get_response(int read_bytes);

    int send_message(std::string message);

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
