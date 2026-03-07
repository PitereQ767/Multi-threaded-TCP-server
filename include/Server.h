#ifndef MULTI_THREADED_TCP_SERVER_SERVER_H
#define MULTI_THREADED_TCP_SERVER_SERVER_H

#include <netinet/in.h>

class Server {
private:
    int server_socket;
    sockaddr_in server_address;

    void handleClient(int client_socket);

public:
    Server(int port);
    virtual ~Server();
    void start_server();
};

#endif //MULTI_THREADED_TCP_SERVER_SERVER_H