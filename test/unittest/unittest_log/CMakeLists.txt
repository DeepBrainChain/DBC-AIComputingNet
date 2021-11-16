cmake_minimum_required (VERSION 3.2)
project(unittest_log)

set(CMAKE_CXX_STANDARD 11)

# include_directories(${CMAKE_SOURCE_DIR}/src/3rd/substrate)

# test asynchronous
# add_definitions(-DASYNCHRONOUS_LOG)

add_executable(${PROJECT_NAME} main.cpp add_file_log.hpp)

target_link_libraries(${PROJECT_NAME}
${Boost_LIBRARIES}
        -pthread -ldl -lz)
