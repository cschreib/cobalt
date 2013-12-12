cmake_minimum_required(VERSION 2.6)
project(refgen)

include_directories(${PROJECT_SOURCE_DIR}/../../common/include)
include_directories(${CLANG_INCLUDE})

add_executable(refgen
    main.cpp
)

target_link_libraries(refgen cobalt-common)
target_link_libraries(refgen ${CLANG_LIBS})

add_custom_command(TARGET refgen POST_BUILD
    COMMAND ${PROJECT_SOURCE_DIR}/../bin/refgen ${PROJECT_SOURCE_DIR}/../../
    -std=c++11
    -I${PROJECT_SOURCE_DIR}/../../common/include
    -I${PROJECT_SOURCE_DIR}/../../server/include
    -I${PROJECT_SOURCE_DIR}/../../client/include
    -I${SFML_INCLUDE_DIR}
    -DNO_AUTOGEN
)
