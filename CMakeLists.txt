cmake_minimum_required(VERSION 3.16)

project(HttpServer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Threads REQUIRED)

add_executable(echo_server
    app/echo_main.cpp
)

add_executable(http_server
    app/http_main.cpp
)

target_link_libraries(echo_server
    PRIVATE
        Threads::Threads
)

target_link_libraries(http_server
    PRIVATE
        Threads::Threads
)