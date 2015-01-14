cmake_minimum_required(VERSION 2.6)
project(generator-test)

include_directories(${PROJECT_SOURCE_DIR}/../include)
include_directories(${PROJECT_SOURCE_DIR}/../../common/include)
include_directories(${PROJECT_SOURCE_DIR}/../../common-netcom/include)
include_directories(${PROJECT_SOURCE_DIR}/../../server/include)
include_directories(${PROJECT_SOURCE_DIR}/../../client/include)
include_directories(${SFML_INCLUDE_DIR})

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${BINARY_DIR}/server/generators")

add_library(generator-test SHARED
    main.cpp
)

set_target_properties(generator-test PROPERTIES PREFIX "")

target_link_libraries(generator-test cobalt-server)
target_link_libraries(generator-test cobalt-client)
target_link_libraries(generator-test cobalt-common-netcom)
target_link_libraries(generator-test cobalt-common)
target_link_libraries(generator-test ${SFML_SYSTEM_LIBRARY})
