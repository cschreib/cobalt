cmake_minimum_required(VERSION 2.6)
project(cobalt-common)

include_directories(${PROJECT_SOURCE_DIR}/include)

set(SRC_LIST 
    ${PROJECT_SOURCE_DIR}/any.cpp
    ${PROJECT_SOURCE_DIR}/config.cpp
    ${PROJECT_SOURCE_DIR}/crc32.cpp
    ${PROJECT_SOURCE_DIR}/netcom_base.cpp
    ${PROJECT_SOURCE_DIR}/packet.cpp
    ${PROJECT_SOURCE_DIR}/space.cpp
    ${PROJECT_SOURCE_DIR}/filesystem_common.cpp
    ${PROJECT_SOURCE_DIR}/string.cpp
    ${PROJECT_SOURCE_DIR}/string_tree.cpp
    ${PROJECT_SOURCE_DIR}/time.cpp
    ${PROJECT_SOURCE_DIR}/xorshift.cpp
)

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    set(SRC_LIST ${SRC_LIST}
        ${PROJECT_SOURCE_DIR}/filesystem_posix.cpp
    )
elseif (${CMAKE_SYSTEM_NAME} MATCHES "Windows")
    set(SRC_LIST ${SRC_LIST}
        ${PROJECT_SOURCE_DIR}/filesystem_win32.cpp
    )
endif()

add_library(cobalt-common STATIC ${SRC_LIST})
