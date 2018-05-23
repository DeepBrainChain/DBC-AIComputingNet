
cmake_minimum_required(VERSION 3.10)

enable_testing()

if (UNIX AND NOT APPLE)
    set(LINUX TRUE)
endif ()

#c++ compiler setting
if (APPLE)
    message(STATUS "apple macosx system")

    set(CMAKE_CXX_FLAGS "-stdlib=libc++ -std=c++11 -DBOOST_LOG_DYN_LINK")
    set(MATRIX_FLAGS "-DMAC_OSX -D_DARWIN_C_SOURCE")
elseif (LINUX)
    message(STATUS "linux system")

    set(CMAKE_CXX_COMPILER "/usr/bin/g++")
    set(MATRIX_FLAGS "-D__linux__")
endif ()

#Setup CMake to run tests

#include_directories(../src})

add_executable(math_test os_math.cpp ../unittest/os_math_test.cpp os_math.h)