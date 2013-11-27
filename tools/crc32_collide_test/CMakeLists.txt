cmake_minimum_required(VERSION 2.6)
project(crc32-collide-test)

include_directories(${PROJECT_SOURCE_DIR}/../../common/include)

add_executable(crc32-collide-test
    main.cpp
)

target_link_libraries(crc32-collide-test cobalt-common)
