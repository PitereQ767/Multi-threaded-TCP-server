#include "include/Server.h"

#include <cstring>
#include <iostream>
#include <sys/socket.h>
#include <unistd.h>
#include <thread>
#include <algorithm> //for std::remove

Server::Server(int port): server_is_running(true) {
    server_socket = socket(AF_INET,SOCK_STREAM,0);
    if (server_socket==-1) {
        std::cerr << "Error creating server socket" << std::endl;
        exit(EXIT_FAILURE);
    }

    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(port);


    if (bind(server_socket, (sockaddr*)&server_address, sizeof(server_address))==-1) {
        std::cerr << "Error binding server port: "<< port<< " !" << std::endl;
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    if (listen(server_socket,5) == -1) {
        std::cerr << "Error listening for connections : " << std::endl;
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    std::cout << "Server configured successfully on port: " << port << std::endl;

    std::thread(&Server::broadcasterThread, this).detach();
}

Server::~Server() {
    std::cout << "Closing the main server socket..." << std::endl;
    server_is_running = false;
    queue_condition.notify_all();
    close(server_socket);
}

void Server::start_server() {
    std::cout << "The server is waiting for a connection..." << std::endl;

    while (true) {
        sockaddr_in client_address{};
        socklen_t client_address_length = sizeof(client_address);

        int client_socket = accept(server_socket, (sockaddr*)&client_address, &client_address_length);
        if (client_socket == -1) {
            std::cerr << "Error accepting client connection!" << std::endl;
            continue;
        }

        std::cout << "Client connected:" << client_socket << std::endl;

        {
            std::lock_guard<std::mutex> lock(client_mutex);
            client_sockets.push_back(client_socket);
        }

        std::thread(&Server::handleClient, this, client_socket).detach();

    }
}

void Server::broadcasterThread() {
    while (server_is_running) {
        std::pair<int, std::string> msg_data;
        {
            std::unique_lock<std::mutex> lock(queue_mutex);

            queue_condition.wait(lock, [this]() {
                return !message_queue.empty() || !server_is_running;
            }); //uzywamy .wait poniewaz gdy zanim watek zostanie uspiony trzeba odblokowac zasob na jakim pracuje

            if (!server_is_running && message_queue.empty()) {
                break;
            }

            msg_data = message_queue.front();
            message_queue.pop();
        }

        int sender_socket = msg_data.first;
        std::string message = msg_data.second;

        std::lock_guard<std::mutex> lock(client_mutex);
        for (int client:client_sockets) {
            if (client != sender_socket || sender_socket == 0) {
                send(client, message.c_str(), message.length(), 0);
            }
        }
    }
}


void Server::handleClient(int client_socket) {
    char buffer[1024];

    while (true) {
        memset(buffer, 0, sizeof(buffer));

        int recived_bytes = recv(client_socket, buffer, sizeof(buffer), 0);

        if (recived_bytes > 0) {
            buffer[recived_bytes] = '\0';
            std::string msg(buffer);

            if (msg.rfind("/nick ", 0) == 0) {
                std::string new_nick = msg.substr(6);

                {
                    std::lock_guard<std::mutex> lock(client_mutex);
                    client_nicks[client_socket] = new_nick;
                }

                std::cout << "Gniazdo " << client_socket << " to teraz " << new_nick << std::endl;
                broadcastUserList();
                std::string welcome_msg = "[System] " + new_nick + " dolaczyl do czatu";
                {
                    std::lock_guard<std::mutex> lock(queue_mutex);
                    message_queue.push({0, welcome_msg});
                }
                queue_condition.notify_one();
            }else {
                std::lock_guard<std::mutex> lock(queue_mutex);
                message_queue.push({client_socket, msg});
                queue_condition.notify_one();
            }


        } else {
            std::string disconnected_nick = "Anyone";
            {
                std::lock_guard<std::mutex> lock(client_mutex);
                if (client_nicks.count(client_socket)) {
                    disconnected_nick = client_nicks[client_socket];
                    client_nicks.erase(client_socket);
                }
            }
            client_sockets.erase(std::remove(client_sockets.begin(), client_sockets.end(), client_socket), client_sockets.end());
            std::cout << "Client " << client_socket << " disconnected" << std::endl;
            broadcastUserList();
            std::string info = "[System] " + disconnected_nick + " left the chat";

            {
                std::lock_guard<std::mutex> lock(queue_mutex);
                message_queue.push({0, info});
            }
            queue_condition.notify_one();
            break;
        }

    }

    close(client_socket);
}

void Server::broadcastUserList() {
    std::string users_msg = "/users ";

    {
        std::lock_guard<std::mutex> lock(client_mutex);
        for (auto const& pair : client_nicks) {
            users_msg += pair.second + ",";
        }
    }

    {
        std::lock_guard<std::mutex> lock(queue_mutex);
        message_queue.push({0, users_msg});
    }
    queue_condition.notify_one();
}
