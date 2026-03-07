#include "include/Server.h"
#include <iostream>
#include <sys/socket.h>
#include <unistd.h>

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

        handleClient(client_socket);
    }
}

void Server::handleClient(int client_socket) {
    char buffer[1024] = {0}; // "{0}" wyzerowanie bufora aby nie znajdowaly sie tam smieci

    int bytes_received = recv(client_socket, buffer, sizeof(buffer), 0);

    if (bytes_received > 0) {
        std::cout << "[Socket "<< client_socket << "] recived: " << buffer << std::endl;

        std::string response = "Server confirms: " + std::string(buffer);
        send(client_socket, response.c_str(), response.length(), 0);
    } else if (bytes_received == 0) {
        std::cout << "[Socket "<< client_socket << "] connection closed" << std::endl;
    } else {
        std::cerr << "Error reading from cient"<<std::endl;
    }

    close(client_socket);
    std::cout << "Customer support has been terminated (Socket ID: "<< client_socket << ")" << std::endl;
    std::cout << "------------------------------------------------------------"<<std::endl;
}
