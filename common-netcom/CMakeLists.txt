cmake_minimum_required(VERSION 2.6)
project(cobalt-common-netcom)

include_directories(${PROJECT_SOURCE_DIR}/../common/include)
include_directories(${PROJECT_SOURCE_DIR}/include)
include_directories(${SFML_INCLUDE_DIR})

set(SRC_LIST
    ${PROJECT_SOURCE_DIR}/any.cpp
    ${PROJECT_SOURCE_DIR}/credential.cpp
    ${PROJECT_SOURCE_DIR}/config_shared_state.cpp
    ${PROJECT_SOURCE_DIR}/netcom_base.cpp
    ${PROJECT_SOURCE_DIR}/packet.cpp
    ${PROJECT_SOURCE_DIR}/serialized_packet.cpp
    ${PROJECT_SOURCE_DIR}/shared_collection.cpp
)

add_library(cobalt-common-netcom STATIC ${SRC_LIST})
