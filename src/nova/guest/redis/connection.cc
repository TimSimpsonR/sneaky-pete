#include "pch.hpp"
#include "connection.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string>
#include <boost/lexical_cast.hpp>
#include <boost/thread.hpp>
#include "nova/redis/RedisException.h"
#include "nova/Log.h"


using boost::lexical_cast;
using boost::optional;
using std::string;


namespace nova { namespace redis {


namespace {


    void *get_in_addr(sockaddr *sa)
    {
        if (sa->sa_family == AF_INET)
        {
            return &(((struct sockaddr_in*)sa)->sin_addr);
        }
        return &(((struct sockaddr_in6*)sa)->sin6_addr);
    }

    int get_socket(std::string host, int port)
    {
        int sockfd, rv;
        struct addrinfo hints, *servinfo, *p;
        char s[INET6_ADDRSTRLEN];
        memset(&hints, 0, sizeof hints);
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        string service_name = lexical_cast<string>(port);
        if ((rv = getaddrinfo(host.c_str(), service_name.c_str(),
                &hints, &servinfo)) != 0)
        {
            perror("Can't find host information\n");
            return -1;
        }
        for(p = servinfo; p != NULL; p = p->ai_next)
        {
            if ((sockfd = socket(p->ai_family, p->ai_socktype,
                    p->ai_protocol)) == -1)
            {
                perror("client: socket\n");
                continue;
            }
            if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1)
            {
                close(sockfd);
                perror("client: connect\n");
                continue;
            }
            break;
        }
        if (p == NULL)
        {
            perror("client: failed to connect\n");
            return -1;
        }
        inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
                    s, sizeof s);
        freeaddrinfo(servinfo);
        return sockfd;
    }

}//end of anon namespace.




Socket::Socket(const std::string & host, const int port,
               const int max_retries)
:   host(host),
    max_retries(max_retries),
    port(port),
    sockfd(-1)
{
}

Socket::~Socket() {
    ::close(sockfd);
}

void Socket::connect() {
    close();
    sockfd = get_socket(host, port);
    if (sockfd < 0) {
        NOVA_LOG_ERROR("Couldn't create connection!");
        throw RedisException(RedisException::CONNECTION_ERROR);
    }
}

void Socket::close() {
    if (sockfd >= 0) {
        NOVA_LOG_TRACE("Closing socket...");
        ::close(sockfd);
        sockfd = -1;
    }
}

void Socket::ensure_connected() {
    if (sockfd < 0) {
        connect();
    }
}

std::string Socket::get_response(int read_bytes) {
    ensure_connected();
    int numbytes;
    char buff[read_bytes+1];
    std::string empty_string = "";
    if (read_bytes < 0)
    {
        return empty_string;
    }
    if ((numbytes = recv(sockfd, buff, read_bytes, 0)) == -1)
    {
        perror("Unable to read from socket!");
        return empty_string;
    }
    buff[numbytes] = '\0';
    std::string data(buff);
    return data;
}

optional<int> Socket::send_message(std::string message) {
    ensure_connected();
    int numbytes;
    int sentbytes = 0;
    int message_length = message.length();
    while (message_length != sentbytes)
    {
        if ((numbytes = send(sockfd, message.c_str(),
                strlen(message.c_str()), 0)) == -1)
        {
            perror("send");
            return boost::none;
        }
        message = message.substr(numbytes, message.length());
        sentbytes += numbytes;
    }
    return sentbytes;
}



} } //end of nova::redis
