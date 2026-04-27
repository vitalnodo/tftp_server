cmake_minimum_required(VERSION 3.10)
project(tftp_server CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(tftp_server
    src/main.cpp
    src/socket.cpp
)

target_include_directories(tftp_server PRIVATE include)
