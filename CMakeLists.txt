cmake_minimum_required(VERSION 3.14)
project(my_raft)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
find_package(Protobuf REQUIRED)
find_package(gRPC CONFIG REQUIRED)

add_library(protolib ${CMAKE_CURRENT_SOURCE_DIR}/proto/raft.proto)
target_link_libraries(protolib gRPC::grpc++  gRPC::grpc++_reflection )
target_include_directories(protolib PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
protobuf_generate(TARGET protolib LANGUAGE cpp)

find_program(GRPC_CPP_PLUGIN NAMES grpc_cpp_plugin)
if(NOT GRPC_CPP_PLUGIN)
    message(FATAL_ERROR "grpc_cpp_plugin not found")
else()
    message(STATUS "Found grpc_cpp_plugin at: ${GRPC_CPP_PLUGIN}")
endif()

protobuf_generate(
        TARGET protolib LANGUAGE grpc
        GENERATE_EXTENSIONS .gprc.pb.h .grpc.pb.cc
        PLUGIN "protoc-gen-grpc=${GRPC_CPP_PLUGIN}"
)


#if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
#    add_compile_options(-Wall -Wextra -Werror)
#elseif (MSVC)
#    add_compile_options(/W4 /WX)
#endif()


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

add_executable(my_raft main.cpp config_parser.cpp raft_server.cpp node.cpp ticker.cpp)

target_link_libraries(my_raft PRIVATE yaml-cpp argparse protolib)