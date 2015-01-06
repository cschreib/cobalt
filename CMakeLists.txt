cmake_minimum_required(VERSION 2.6)
project(cobalt)

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    add_definitions(-Weverything)
    add_definitions(-Wno-c++98-compat-pedantic)
    add_definitions(-Wno-c++98-compat)
    add_definitions(-Wno-unused-parameter)
    add_definitions(-Wno-sign-conversion)
    add_definitions(-Wno-conversion)
    add_definitions(-Wno-missing-prototypes)
    add_definitions(-Wno-missing-variable-declarations)
    add_definitions(-Wno-padded)
    add_definitions(-Wno-float-equal)
    add_definitions(-Wno-unused-variable)
    add_definitions(-Wno-global-constructors)
    add_definitions(-Wno-exit-time-destructors)
    add_definitions(-Wno-shadow)
    add_definitions(-Wno-weak-vtables)
    add_definitions(-Wno-covered-switch-default)
    add_definitions(-Wno-documentation-unknown-command)
    add_definitions(-Wno-unneeded-internal-declaration)
    add_definitions(-Wno-unused-function)
    add_definitions(-Wno-unused-macros)
    add_definitions(-Wno-switch-enum)
    add_definitions(-Wno-disabled-macro-expansion)
    if(CMAKE_BUILD_TYPE MATCHES Debug)
        add_definitions(-g)
    endif()
    add_definitions(-O2)
    add_definitions(-std=c++11)
    add_definitions(-ftemplate-backtrace-limit=0)
    add_definitions(-ferror-limit=5)
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    add_definitions(-Wall)
    if(CMAKE_BUILD_TYPE MATCHES Debug)
        add_definitions(-g)
    endif()
    add_definitions(-O2)
    add_definitions(-std=c++11)
    add_definitions(-fmax-errors=5)
    add_definitions(-Wno-unused-variable)
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Intel")
  # using Intel C++
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
  # using Visual Studio C++
endif()

if (UNIX)
    add_definitions(-DPOSIX)
endif()
if (WINDOWS)
    add_definitions(-DWIN32)
endif()

file(MAKE_DIRECTORY "${PROJECT_SOURCE_DIR}/bin")
file(MAKE_DIRECTORY "${PROJECT_SOURCE_DIR}/tools/bin")

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${PROJECT_SOURCE_DIR}/cmake")
set(BINARY_DIR "${PROJECT_SOURCE_DIR}/bin")

find_package(SFML 2 COMPONENTS system network)
find_package(LibClang 3)
find_package(lxgui 1)
find_package(TBB 4)

if (UNIX)
    find_package(LibDL)
endif()

# build the common library
add_subdirectory(common)

# build and launch refgen
add_subdirectory(tools/refgen)

# build the common netcom library
add_subdirectory(common-netcom)

# build server & client libraries
add_subdirectory(server)
add_subdirectory(client)

# build server & client frontends
add_subdirectory(server-cli)
add_subdirectory(client-cli)

# build generators
add_subdirectory(generators/test)

# enable documentation build
add_custom_target(doxygen COMMAND doxygen doxygen.conf
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/doc)
