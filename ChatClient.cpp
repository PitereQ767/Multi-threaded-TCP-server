#include "include/ChatClient.h"

#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h> //for close
#include "imgui/imgui.h"
#include <iostream>
#include <chrono>
#include <iomanip>
#include <sstream>

ChatClient::ChatClient() {
    client_socket = -1;
    is_connected = false;
    is_running = true;

    memset(ip_buffer, 0, sizeof(ip_buffer));
    strcpy(ip_buffer, "127.0.0.1");

    memset(port_buffer, 0, sizeof(port_buffer));
    strcpy(port_buffer, "8080");

    memset(nick_buffer, 0, sizeof(nick_buffer));
    strcpy(nick_buffer, "User");

    memset(message_buffer, 0, sizeof(message_buffer));
}

ChatClient::~ChatClient() {
    is_running = false;

    if (client_socket != -1) {
        close(client_socket);
    }

    if (receive_thread.joinable()) {
        receive_thread.join();
    }
}

void ChatClient::addLog(const std::string &message, MsgType type) {
    auto now = std::chrono::system_clock::now();
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);
    std::tm* now_tm = std::localtime(&now_c);

    std::stringstream ss;
    ss << "[" << std::put_time(now_tm, "%H:%M:%S") << "] " << message;

    std::lock_guard<std::mutex> lock(chat_history_mutex);
    chat_history.push_back({ss.str(), type});
}

void ChatClient::receiveMessages() {
    char buffer[1024];
    while (is_running && is_connected) {
        memset(buffer, 0, sizeof(buffer));

        int bytes_recieved = recv(client_socket, buffer, sizeof(buffer) - 1, 0);

        if (bytes_recieved > 0) {
            buffer[bytes_recieved] = '\0';

            std::string rec_str(buffer);

            std::stringstream packet_stream(rec_str);
            std::string single_message;

            while (std::getline(packet_stream, single_message, '\n')) {
                if (single_message.empty()) continue;

                if (single_message.rfind("/users ", 0) == 0) {
                    std::string users_str = single_message.substr(7);

                    std::vector<std::string> new_users;
                    std::stringstream ss(users_str);
                    std::string one_name;

                    while (std::getline(ss, one_name, ',')) {
                        if (!one_name.empty()) new_users.push_back(one_name);
                    }

                    std::lock_guard<std::mutex> lock(users_mutex);
                    active_users = new_users;
                }else if (single_message.rfind("[System] ", 0) == 0) {
                    std::string sub_msg = single_message.substr(9);
                    addLog(sub_msg, MsgType::SYSTEM);
                }else if (single_message.rfind("[Szept", 0) == 0) {
                    addLog(single_message, MsgType::WHISPER);
                }else if (single_message.rfind("[History", 0) == 0) {
                    addLog(single_message, MsgType::HISTORY);
                }
                else {
                    addLog(single_message, MsgType::OTHER);
                }
            }
        }else if (bytes_recieved == 0) {
            addLog("The Client has lost the connection", MsgType::SYSTEM);
            is_connected = false;
            break;
        }else {
            break;
        }
    }
}

void ChatClient::normalMessage() {
    std::string full_msg = std::string(nick_buffer) + ": " + std::string(message_buffer);

    addLog(full_msg, MsgType::ME);

    send(client_socket, full_msg.c_str(), full_msg.length(), 0);

    memset(message_buffer, 0, sizeof(message_buffer));

    ImGui::SetKeyboardFocusHere(-1); //przywrocenie kursora do pola tekstowego
}

void ChatClient::privateMessage(const std::string &typed_text) {
    size_t space_pos = typed_text.find(' ', 3);

    if (space_pos != std::string::npos) {
        std::string target = typed_text.substr(3, space_pos - 3);
        std::string content = typed_text.substr(space_pos + 1);

        std::string local_echo = "[Whisper to " + target + "] " + content + "\n";
        addLog(local_echo, MsgType::WHISPER);

        std::string network_msg = "/w " + target + " [Whisper from " + std::string(nick_buffer) + "] " + content + "\n";
        send(client_socket, network_msg.c_str(), network_msg.length(), 0);

        //Czyszczenie
        memset(message_buffer, 0, sizeof(message_buffer));

        ImGui::SetKeyboardFocusHere(-1);
    }else {
        addLog("Wrong format! Use: /w Nick Message", MsgType::SYSTEM);
    }
}

void ChatClient::drawUI() {
    if (!is_connected) {
        ImGui::Begin("Login to the server", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::InputText("Address IP", ip_buffer, sizeof(ip_buffer));
        ImGui::InputText("Port", port_buffer, sizeof(port_buffer));
        ImGui::InputText("Nick", nick_buffer, sizeof(nick_buffer));

        if (ImGui::Button("Connect", ImVec2(120, 0))) {
            client_socket = socket(AF_INET, SOCK_STREAM, 0);
            if (client_socket != -1) {
                sockaddr_in server_addr;
                server_addr.sin_family = AF_INET;
                server_addr.sin_port = htons(std::stoi(port_buffer));
                inet_pton(AF_INET, ip_buffer, &server_addr.sin_addr); //konwertuje IP na binarna postac numeryczna

                if (connect(client_socket, (sockaddr*)&server_addr, sizeof(server_addr)) == 0) {
                    is_connected = true;
                    addLog(("Connected to the server " + std::string(ip_buffer)), MsgType::SYSTEM);

                    std::string hello = "/nick " + std::string(nick_buffer);
                    send(client_socket, hello.c_str(), hello.length(), 0);

                    receive_thread = std::thread(&ChatClient::receiveMessages, this);
                }else {
                    close(client_socket);
                    client_socket = -1;
                }
            }
        }

        ImGui::End();
    }else {

        ImGui::SetNextWindowSize(ImVec2(600, 400), ImGuiCond_Appearing);
        ImGui::Begin("Chat TCP", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::BeginChild("ChatHistory", ImVec2(ImGui::GetWindowWidth() * 0.70f, -ImGui::GetFrameHeightWithSpacing()), true);

        {
            std::lock_guard<std::mutex> lock(chat_history_mutex);
            for (const auto & msg : chat_history) {
                ImVec4 color;
                if (msg.type == MsgType::SYSTEM) {
                    color = ImVec4(1.0f, 1.0f, 0.0f, 1.0f);
                }else if (msg.type == MsgType::ME) {
                    color = ImVec4(0.0f, 1.0f, 0.5f, 1.0f);
                }else if (msg.type == MsgType::WHISPER) {
                  color = ImVec4(1.0f, 0.5f, 1.0f, 1.0f);
                }else if (msg.type == MsgType::HISTORY) {
                    color = ImVec4(0.7f, 0.7f, 0.7f, 1.0f);
                }else {
                    color = ImVec4(0.4f, 0.8f, 1.0f, 1.0f);
                }
                ImGui::TextColored(color, "%s", msg.text.c_str());
            }

            if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
                ImGui::SetScrollHereY(1.0f);
            }
        }
        ImGui::EndChild();

        ImGui::SameLine();
        ImGui::BeginChild("UserList", ImVec2(0, -ImGui::GetFrameHeightWithSpacing()), true);
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Active ( %zu )", active_users.size());
        ImGui::Separator();

        {
            std::lock_guard<std::mutex> lock(users_mutex);
            for (const auto& user : active_users) {
                ImGui::BulletText("%s", user.c_str());
            }
        }
        ImGui::EndChild();

        bool send_pressed = ImGui::InputText("##Messsage", message_buffer, sizeof(message_buffer), ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::SameLine();
        send_pressed |= ImGui::Button("Send");

        if (send_pressed && strlen(message_buffer) > 0) {
            std::string typed_text(message_buffer);
            if (typed_text.rfind("/w ", 0) == 0) {
                privateMessage(typed_text);
            }else {
                normalMessage();
            }
        }

        ImGui::End();
    }
}
