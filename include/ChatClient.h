#ifndef MULTI_THREADED_TCP_SERVER_CHATCLIENT_H
#define MULTI_THREADED_TCP_SERVER_CHATCLIENT_H

#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>


class ChatClient {
    int client_socket;
    std::atomic<bool> is_connected;
    std::atomic<bool> is_running;

    //ImGui requires character array text
    char ip_buffer[64];
    char port_buffer[16];
    char message_buffer[512];

    std::vector<std::string> chat_history;
    std::mutex chat_history_mutex;

    std::thread receive_thread;

    void receiveMessages();

    void addLog(const std::string& message);

public:
    ChatClient();
    ~ChatClient();

    void drawUI();
};

#endif //MULTI_THREADED_TCP_SERVER_CHATCLIENT_H