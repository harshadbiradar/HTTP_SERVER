cmake_minimum_required(VERSION 3.16)
project(Concurrent_Server LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(concurrent_server)

target_sources(concurrent_server PRIVATE
    src/thread_pool.cpp
    src/connection_manager.cpp
    src/server.cpp
    src/main.cpp
)

target_include_directories(concurrent_server PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

target_compile_options(concurrent_server PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -g

    -fsanitize=address,undefined
    -fno-omit-frame-pointer
)

target_link_options(concurrent_server PRIVATE
    -fsanitize=address,undefined
)