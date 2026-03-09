cmake_minimum_required(VERSION 4.1)
project(Multi_threaded_TCP_server)

set(CMAKE_CXX_STANDARD 14)

add_executable(Multi_threaded_TCP_server
        main.cpp
        include/Server.h
        Server.cpp
        Client.cpp)
