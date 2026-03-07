#ifndef MULTI_THREADED_TCP_SERVER_SERVER_H
#define MULTI_THREADED_TCP_SERVER_SERVER_H

#include <netinet/in.h>
#include <vector>
#include <mutex>
#include <string>

class Server {
private:
    int server_socket;
    sockaddr_in server_address;
    std::vector<int> client_sockets;
    std::mutex client_mutex;

    void handleClient(int client_socket);
    void broadcastMessage(std::string& message, int sender_socket);

public:
    Server(int port);
    virtual ~Server();
    void start_server();
};

#endif //MULTI_THREADED_TCP_SERVER_SERVER_H