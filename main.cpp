#include <iostream>
#include "include/Server.h"
int main() {
    std::cout << "Initialize chat server..." << std::endl;

    Server myChatServer(8080);
    // myChatServer.start_server();

    return 0;
}