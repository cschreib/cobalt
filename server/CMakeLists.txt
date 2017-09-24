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
    server_serializable.cpp
    server_state.cpp
    server_state_configure.cpp
    server_state_game.cpp
    server_state_idle.cpp
    server_player_list.cpp
    server_universe.cpp
    server_space_object.cpp
)

if (CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
  set_target_properties(${PROJECT_NAME} PROPERTIES COMPILE_FLAGS "-fPIC")
endif()
