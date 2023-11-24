cmake_minimum_required(VERSION 3.26)
project(ServidorTFTP)

set(CMAKE_CXX_STANDARD 17)

add_executable(ServidorTFTP main.cpp
        TftpServer.cpp
        TftpServer.h
)
