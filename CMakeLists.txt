cmake_minimum_required (VERSION 3.2)
project("dbc")

set(VERSION_MAJOR 0)
set(VERSION_MINOR 2)
set(VERSION_PATCH 0)
set(VERSION "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}")

set(CMAKE_CXX_STANDARD 11)

set(CMAKE_CXX_FLAGS "-fPIC -w")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS} -g")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS} -O2")

include(cmake/compiler_option.cmake)
include(cmake/find_external_lib.cmake)

SET(HOME_PATH ${CMAKE_SOURCE_DIR})

if ($ENV{DBC_OUTPUT})
    SET(EXECUTABLE_OUTPUT_PATH $ENV{DBC_OUTPUT}/output)
    SET(LIBRARY_OUTPUT_PATH $ENV{DBC_OUTPUT}/output)
else ()
    SET(EXECUTABLE_OUTPUT_PATH ${HOME_PATH}/output)
    SET(LIBRARY_OUTPUT_PATH ${HOME_PATH}/output)
endif ()

add_subdirectory("src")
add_subdirectory("src/core")
#add_subdirectory("src/service_core")

add_dependencies(dbc dbc_core)
#add_dependencies(dbc_service_core dbc_core)

# unittest
#add_subdirectory("src/core/unittest")
#add_subdirectory("src/core/unittest_network")
#add_subdirectory("src/service_core/unittest")
#add_subdirectory("src/service/unittest")

#add_dependencies(dbc_core_test dbc_core)
#add_dependencies(dbc_service_core_test dbc_core dbc_service_core)
#add_dependencies(dbc_service_test dbc_core dbc_service_core)
