#ifndef MULTI_THREADED_TCP_SERVER_CHATCLIENT_H
#define MULTI_THREADED_TCP_SERVER_CHATCLIENT_H

#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>

enum class MsgType {
    SYSTEM,
    ME,
    OTHER,
    WHISPER
};

struct ChatMessage {
    std::string text;
    MsgType type;
};


class ChatClient {
    int client_socket;
    std::atomic<bool> is_connected;
    std::atomic<bool> is_running;

    //ImGui requires character array text
    char ip_buffer[64];
    char port_buffer[16];
    char message_buffer[512];
    char nick_buffer[32];

    std::vector<ChatMessage> chat_history;
    std::mutex chat_history_mutex;

    std::vector<std::string> active_users;
    std::mutex users_mutex;

    std::thread receive_thread;

    void receiveMessages();

    void addLog(const std::string& message, MsgType type);

    void normalMessage();
    void privateMessage(const std::string &typed_text);

public:
    ChatClient();
    ~ChatClient();

    void drawUI();
};

#endif //MULTI_THREADED_TCP_SERVER_CHATCLIENT_H