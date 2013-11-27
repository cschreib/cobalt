cmake_minimum_required(VERSION 2.6)
project(cobalt-server)

include_directories(${PROJECT_SOURCE_DIR}/include)
include_directories(${PROJECT_SOURCE_DIR}/../common/include)
include_directories(${PROJECT_SOURCE_DIR}/../client/include)

add_library(cobalt-server STATIC
    server_netcom.cpp
)
