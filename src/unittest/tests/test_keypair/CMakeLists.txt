cmake_minimum_required (VERSION 3.2)
project(test_keypair)

set(CMAKE_CXX_STANDARD 11)

include_directories(${CMAKE_SOURCE_DIR}/src/3rd/substrate)

add_executable(test_keypair test.cpp)

target_link_libraries(test_keypair
        ${CMAKE_SOURCE_DIR}/src/3rd/lib/linux/substrate/libsc_cli.a
        -pthread -ldl -lz)
