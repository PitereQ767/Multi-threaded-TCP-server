#ifndef MULTI_THREADED_TCP_SERVER_SERVER_H
#define MULTI_THREADED_TCP_SERVER_SERVER_H

#include <netinet/in.h>
#include <vector>
#include <mutex>
#include <string>
#include <queue>
#include <condition_variable>
#include <map>
#include <sqlite3.h>

class Server {
private:
    int server_socket;
    sockaddr_in server_address;
    bool server_is_running;

    std::vector<int> client_sockets;
    std::mutex client_mutex;
    std::map<int, std::string> client_nicks;

    std::queue<std::pair<int, std::string>> message_queue;
    std::mutex queue_mutex;
    std::condition_variable queue_condition; // CPU don't use 100% of power

    sqlite3 *db;

    void handleClient(int client_socket);
    // void broadcastMessage(std::string& message, int sender_socket);
    void broadcasterThread();
    void broadcastUserList();

    void initDatabase();
public:
    Server(int port);
    virtual ~Server();
    void start_server();
};

#endif //MULTI_THREADED_TCP_SERVER_SERVER_H