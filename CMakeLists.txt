cmake_minimum_required (VERSION 3.2)
project("dbcproj")

set(CMAKE_CXX_STANDARD 11)

include(cmake/compiler_option.cmake)
include(cmake/find_external_lib.cmake)

add_subdirectory(src)

# unittest
#add_subdirectory("src/core/unittest")
#add_subdirectory("src/core/unittest_network")
#add_subdirectory("src/service_core/unittest")
#add_subdirectory("src/service/unittest")

#add_dependencies(dbc_core_test dbc_core)
#add_dependencies(dbc_service_core_test dbc_core dbc_service_core)
#add_dependencies(dbc_service_test dbc_core dbc_service_core)
