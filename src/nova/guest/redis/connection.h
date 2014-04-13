#ifndef CONNECTION_H
#define CONNECTION_H

#include <string>

namespace nova { namespace redis {



int get_socket(std::string host, std::string port);

int send_message(int sockfd, std::string message);

std::string get_response(int sockfd, int read_bytes);


}}//nova::redis
#endif /* CONNECTION_H */
