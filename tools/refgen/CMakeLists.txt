cmake_minimum_required(VERSION 2.6)
project(refgen)

include_directories(${PROJECT_SOURCE_DIR}/../../common/include)
include_directories(${PROJECT_SOURCE_DIR}/../../common-netcom/include)
include_directories(${CLANG_INCLUDE})
include_directories(${SFML_INCLUDE_DIR})
add_definitions(-DNO_AUTOGEN)

add_executable(refgen
    main.cpp
)

target_link_libraries(refgen cobalt-common)
target_link_libraries(refgen ${CLANG_LIBS})
target_link_libraries(refgen ${SFML_NETWORK_LIBRARY})
target_link_libraries(refgen ${SFML_SYSTEM_LIBRARY})

if(CMAKE_BUILD_TYPE MATCHES Debug)
    set(REFGEN_BUILD_TYPE "debug")
else()
    set(REFGEN_BUILD_TYPE "release")
endif()

add_custom_target(refgen-gen ALL
    COMMAND ${PROJECT_SOURCE_DIR}/../bin/refgen
     ${REFGEN_BUILD_TYPE} ${PROJECT_SOURCE_DIR}/../../
    -std=c++11
    -I${PROJECT_SOURCE_DIR}/../../common/include
    -I${PROJECT_SOURCE_DIR}/../../common-netcom/include
    -I${PROJECT_SOURCE_DIR}/../../server/include
    -I${PROJECT_SOURCE_DIR}/../../client/include
    -I${SFML_INCLUDE_DIR}
    -DNO_AUTOGEN
)

add_custom_target(refgen-clean COMMAND ${PROJECT_SOURCE_DIR}/../bin/refgen
    clean ${PROJECT_SOURCE_DIR}/../../)
