cmake_minimum_required (VERSION 3.2)

############################################################################

#debug setting
set(DEBUG_MODE ON)
set(CMAKE_VERBOSE_MAKEFILE on)

project("dbc")

set(VERSION_MAJOR 0)
set(VERSION_MINOR 2)
set(VERSION_PATCH 0)
set(VERSION "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}")

#set(CMAKE_CXX_STANDARD 11)

include(cmake/compiler_option.cmake)
include(cmake/find_external_lib.cmake)


# dbc executable
add_subdirectory("src")

# library
add_subdirectory("src/core")
add_subdirectory("src/service_core")

# unit test
add_subdirectory("src/core/unittest")
add_subdirectory("src/service_core/unittest")

# will generate all dependency before genereate the target
add_dependencies(dbc dbc_core dbc_service_core)
add_dependencies(dbc_core_test dbc_core)
add_dependencies(dbc_service_core_test dbc_core dbc_service_core)
