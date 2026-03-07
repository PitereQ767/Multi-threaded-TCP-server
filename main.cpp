#include <iostream>
#include <csignal>
#include "include/Server.h"

Server* global_server = nullptr;

void handle_sigint(int signal) {
    if (global_server != nullptr) {
        delete global_server;
    }

    std::cout << "Server turn off. Good bye!" << std::endl;
    exit(0);
}


int main() {
    std::signal(SIGINT, handle_sigint);

    std::cout << "Initialize chat server..." << std::endl;

    global_server = new Server(8080);
    global_server->start_server();

    return 0;
}