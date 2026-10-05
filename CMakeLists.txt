cmake_minimum_required(VERSION 3.16)

project(HttpServer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(base STATIC
    src/base/any.cpp
    src/base/buffer.cpp
)
target_include_directories(base PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include/base
)

add_executable(tmp app/tmp.cpp)
target_link_libraries(tmp PRIVATE base)
