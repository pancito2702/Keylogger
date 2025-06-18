cmake_minimum_required(VERSION 3.10)
project(Cliente)

add_executable(Cliente main.cpp)

target_link_libraries(Cliente
    ws2_32
    mswsock
    advapi32
)