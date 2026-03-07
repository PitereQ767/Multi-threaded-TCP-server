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
