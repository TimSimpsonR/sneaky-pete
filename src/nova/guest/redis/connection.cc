#include "pch.hpp"
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


}//end of anon namespace.


int get_socket(std::string host, std::string port)
{
    int sockfd, rv;
    struct addrinfo hints, *servinfo, *p;
    char s[INET6_ADDRSTRLEN];
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    if ((rv = getaddrinfo(host.c_str(), port.c_str(),
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

int send_message(int sockfd, std::string message)
{
    int numbytes;
    int sentbytes = 0;
    int message_length = message.length();
    while (message_length != sentbytes)
    {
        if ((numbytes = send(sockfd, message.c_str(),
                strlen(message.c_str()), 0)) == -1)
        {
            perror("send");
            return -1;
        }
        message = message.substr(numbytes, message.length());
        sentbytes += numbytes;
    }
    return sentbytes;
}

std::string get_response(int sockfd, int read_bytes)
{
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


}}//end of nova::redis
