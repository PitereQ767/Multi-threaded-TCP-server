#include "include/Server.h"

#include <cstring>
#include <iostream>
#include <sys/socket.h>
#include <unistd.h>
#include <thread>
#include <algorithm> //for std::remove

Server::Server(int port) {
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
}

Server::~Server() {
    std::cout << "Closing the main server socket..." << std::endl;
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

void Server::broadcastMessage(std::string &message, int sender_socket) {
    std::lock_guard<std::mutex> lock(client_mutex);

    for (int client:client_sockets) {
        if (client != sender_socket) {
            send(client, message.c_str(), message.length(),0 );
        }
    }
}

void Server::handleClient(int client_socket) {
    char buffer[1024];

    while (true) {
        memset(buffer, 0, sizeof(buffer));

        int recived_bytes = recv(client_socket, buffer, sizeof(buffer), 0);

        if (recived_bytes > 0) {
            std::string msg = "[Klient " + std::to_string(client_socket) + "]" + std::string(buffer);
            std::cout << msg << std::endl;

            broadcastMessage(msg, client_socket);
        } else {
            std::cout << "Client disconnected" << std::endl;
            break;
        }

    }

    close(client_socket);
    {
        std::lock_guard<std::mutex> lock(client_mutex);
        client_sockets.erase(std::remove(client_sockets.begin(), client_sockets.end(), client_socket), client_sockets.end()); //"Erase-Remove Idiom"
    }
}
