cmake_minimum_required(VERSION 3.14)
project(my_raft)

set(CMAKE_CXX_STANDARD 17)

include(FetchContent)

FetchContent_Declare(
        yaml-cpp
        GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
        GIT_TAG 0.8.0
)
FetchContent_MakeAvailable(yaml-cpp)

FetchContent_Declare(
        argparse
        GIT_REPOSITORY https://github.com/p-ranav/argparse.git
        GIT_TAG v2.5
)
FetchContent_MakeAvailable(argparse)

add_executable(my_raft main.cpp config_parser.cpp)

target_link_libraries(my_raft PRIVATE yaml-cpp argparse)
