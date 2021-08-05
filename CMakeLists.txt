cmake_minimum_required (VERSION 3.2)
project("dbcproj")

set(CMAKE_CXX_STANDARD 11)

include(cmake/compiler_option.cmake)
include(cmake/find_external_lib.cmake)

add_subdirectory(src)
