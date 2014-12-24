cmake_minimum_required(VERSION 2.6)
project(cobalt-server)

include_directories(${PROJECT_SOURCE_DIR}/include)
include_directories(${PROJECT_SOURCE_DIR}/../common/include)
include_directories(${PROJECT_SOURCE_DIR}/../common-netcom/include)
include_directories(${PROJECT_SOURCE_DIR}/../client/include)
include_directories(${SFML_INCLUDE_DIR})

add_library(cobalt-server STATIC
    server_instance.cpp
    server_netcom.cpp
    server_state_configure.cpp
    server_state_test.cpp
    server_player_list.cpp
)
