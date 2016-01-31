cmake_minimum_required(VERSION 2.6)
project(cobalt-client)

include_directories(${PROJECT_SOURCE_DIR}/include)
include_directories(${PROJECT_SOURCE_DIR}/../common/include)
include_directories(${PROJECT_SOURCE_DIR}/../common-netcom/include)
include_directories(${PROJECT_SOURCE_DIR}/../server/include)
include_directories(${SFML_INCLUDE_DIR})

add_library(cobalt-client STATIC
    client_netcom.cpp
    client_player_list.cpp
)

if (CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
  set_target_properties(${PROJECT_NAME} PROPERTIES COMPILE_FLAGS "-fPIC")
endif()
