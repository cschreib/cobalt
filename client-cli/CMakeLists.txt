cmake_minimum_required(VERSION 2.6)
project(cobalt-client-cli)

include_directories(${PROJECT_SOURCE_DIR}/include)
include_directories(${PROJECT_SOURCE_DIR}/../common/include)
include_directories(${PROJECT_SOURCE_DIR}/../common-netcom/include)
include_directories(${PROJECT_SOURCE_DIR}/../client/include)
include_directories(${PROJECT_SOURCE_DIR}/../server/include)
include_directories(${LUABIND_INCLUDE_DIRS})
include_directories(${SFML_INCLUDE_DIR})
include_directories(${TBB_INCLUDE_DIR})


set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${BINARY_DIR}/client")

add_executable(cobalt-client-cli
    sfml_wrapper.cpp
    console_input.cpp
    console_output.cpp
    work_loop.cpp
    main.cpp
)

target_link_libraries(cobalt-client-cli cobalt-client)
target_link_libraries(cobalt-client-cli cobalt-server)
target_link_libraries(cobalt-client-cli cobalt-common-netcom)
target_link_libraries(cobalt-client-cli cobalt-common)
target_link_libraries(cobalt-client-cli ${SFML_GRAPHICS_LIBRARY})
target_link_libraries(cobalt-client-cli ${SFML_WINDOW_LIBRARY})
target_link_libraries(cobalt-client-cli ${SFML_NETWORK_LIBRARY})
target_link_libraries(cobalt-client-cli ${SFML_SYSTEM_LIBRARY})
target_link_libraries(cobalt-client-cli ${LUABIND_LIBRARIES})
target_link_libraries(cobalt-client-cli ${TBB_LIBRARY})

if (UNIX)
    target_link_libraries(cobalt-client-cli ${LIBDL_LIBRARY})
endif()
