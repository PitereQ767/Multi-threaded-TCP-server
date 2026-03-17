#include "include/Server.h"

#include <cstring>
#include <iostream>
#include <sys/socket.h>
#include <unistd.h>
#include <thread>
#include <algorithm> //for std::remove

Server::Server(int port): server_is_running(true) {
    int db_r = sqlite3_open("chat_history.db", &db);
    if (db_r != SQLITE_OK) {
        std::cerr << "Error opening database: " << sqlite3_errmsg(db) << std::endl;
        exit(EXIT_FAILURE);
    }else {
        std::cout << "Successfully opened database" << std::endl;
    }

    initDatabase();

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

    sqlite3_close(db);
    std::cout << "Closed connection with database." << std::endl;
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

        message += "\n";

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

        int recived_bytes = recv(client_socket, buffer, sizeof(buffer) - 1, 0);

        if (recived_bytes > 0) {
            buffer[recived_bytes] = '\0';
            std::string msg(buffer);

            if (msg.rfind("/nick ", 0) == 0) {
                std::string new_nick = msg.substr(6);

                {
                    std::lock_guard<std::mutex> lock(client_mutex);
                    client_nicks[client_socket] = new_nick;
                }

                std::cout << "Socket " << client_socket << " is now " << new_nick << std::endl;
                broadcastUserList();

                readFromDatabase(client_socket);

                std::string welcome_msg = "[System] " + new_nick + " join to chat";
                {
                    std::lock_guard<std::mutex> lock(queue_mutex);
                    message_queue.push({0, welcome_msg});
                }
                queue_condition.notify_one();
            }else if (msg.rfind("/w ", 0) == 0) {
                size_t space_pos = msg.find(" ", 3);
                if (space_pos != std::string::npos) {
                    std::string target_nick = msg.substr(3, space_pos - 3);
                    std::string actual_msg = msg.substr(space_pos + 1) + "\n";

                    int target_socket = -1;

                    {
                        std::lock_guard<std::mutex> lock(client_mutex);
                        for (const auto& pair : client_nicks) {
                            if (pair.second == target_nick) {
                                target_socket = pair.first;
                            }
                        }
                    }

                    if (target_socket != -1) {
                        // std::cout << actual_msg << std::endl;
                        send(target_socket, actual_msg.c_str(), actual_msg.length(), 0);
                    }else {
                        std::string err_msg = "[System] User " + target_nick + " dosen't exist \n";
                        send(client_socket, err_msg.c_str(), err_msg.length(), 0);
                    }
                }
            }
            else {
                saveToDatabase(client_socket, msg);
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

void Server::initDatabase() {
    const char* sql = "CREATE TABLE IF NOT EXISTS messages ("
                      "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                      "sender TEXT NOT NULL, "
                      "content TEXT NOT NULL, "
                      "timestamp DATETIME DEFAULT CURRENT_TIMESTAMP);";

    char* error_msg = nullptr;

    int rc = sqlite3_exec(db, sql, nullptr, nullptr, &error_msg);
    if (rc != SQLITE_OK) {
        std::cerr << "Error in SQL: " << error_msg << std::endl;
        sqlite3_free(error_msg);
    }else {
        std::cout << "Table 'messages' ready to use." << std::endl;
    }
}

void Server::saveToDatabase(int client_socket, const std::string& msg) {
    std::string current_nick = "Anyone";
    {
        std::lock_guard<std::mutex> lock(client_mutex);
        if (client_nicks.count(client_socket)) {
            current_nick = client_nicks[client_socket];
        }
    }
    std::string sql = "INSERT INTO messages (sender, content) VALUES (?, ?);";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        //Zabezpiecznie przed niebezpiecznymi dla bazy wiadomosciami
        sqlite3_bind_text(stmt, 1, current_nick.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, msg.c_str(), -1, SQLITE_TRANSIENT);

        if (sqlite3_step(stmt) != SQLITE_DONE) {
            std::cerr << "Error in save to database" << std::endl;
        }

        sqlite3_finalize(stmt);
    }else {
        std::cerr << "Error copilation SQL" << std::endl;
    }
}

void Server::readFromDatabase(int client_socket) {
    std::string history_sql = "SELECT sender, content FROM (SELECT * FROM messages ORDER BY id DESC LIMIT 50) ORDER BY id ASC;";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db, history_sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            const char* db_sender = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            const char* db_content = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));

            if (db_sender && db_content) {
                std::string history_msg = "[History] " + std::string(db_sender) + ": " + std::string(db_content) + "\n";

                send(client_socket, history_msg.c_str(), history_msg.length(), 0);
            }
        }
        sqlite3_finalize(stmt);
    }else {
        std::cerr << "Error in read history from database" << std::endl;
    }


}
