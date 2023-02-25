cmake_minimum_required(VERSION 3.10)
project(bbserv)

set(CMAKE_CXX_STANDARD 14)

add_subdirectory(BbServer)
add_subdirectory(Timer)
add_subdirectory(Thread)

add_executable(bbserv main_server.cpp)

target_link_libraries(bbserv BbServer Timer Thread)
