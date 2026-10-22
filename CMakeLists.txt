cmake_minimum_required(VERSION 3.16)

project(HttpServer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Threads REQUIRED)
# base类 1.添加静态库 2.添加头文件 3.链接成库
add_library(base STATIC
libs/base/src/buffer.cpp
libs/base/src/any.cpp
)
target_include_directories(base PUBLIC
    libs/base/include
)
target_link_libraries(base PUBLIC Threads::Threads)

# net类：Reactor网络模块
add_library(net STATIC 
    libs/net/src/acceptor.cpp
    libs/net/src/channel.cpp
    libs/net/src/connection.cpp
    libs/net/src/eventLoop.cpp
    libs/net/src/loopThread.cpp
    libs/net/src/loopThreadpool.cpp
    libs/net/src/poller.cpp
    libs/net/src/socket.cpp
    libs/net/src/tcpServer.cpp
    libs/net/src/timeWheel.cpp
)
target_include_directories(net PUBLIC 
libs/net/include
)
target_link_libraries(net PUBLIC base)

# http类：HTTP解析与服务器
add_library(http STATIC
    libs/http/src/http_context.cpp
    libs/http/src/http_request.cpp
    libs/http/src/http_response.cpp
    libs/http/src/http_server.cpp
    libs/http/src/http_utility.cpp
)
target_include_directories(http PUBLIC 
libs/http/include
)
target_link_libraries(http PUBLIC net)

# 生成三个可执行目标
add_executable(tmp app/tmp.cpp)
target_link_libraries(tmp PRIVATE base)

add_executable(http_server app/http_server.cpp)
target_link_libraries(http_server PRIVATE http)

add_executable(echo_server app/echo_server.cpp)
target_link_libraries(echo_server PRIVATE net)

include(CTest)
if(BUILD_TESTING)
    add_executable(http_request_test tests/http_request_test.cpp)
    target_link_libraries(http_request_test PRIVATE http)
    add_test(NAME http_request_test COMMAND http_request_test)

    add_executable(timer_wheel_test tests/timer_wheel_test.cpp)
    target_link_libraries(timer_wheel_test PRIVATE net)
    add_test(NAME timer_wheel_test COMMAND timer_wheel_test)
endif()
