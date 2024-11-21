cmake_minimum_required (VERSION 3.2)
project(deeplink_lan_server)

set(CMAKE_CXX_STANDARD 11)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/output)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/output)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/output)

# include_directories(${CMAKE_SOURCE_DIR}/src/3rd/substrate)
include_directories(${CMAKE_SOURCE_DIR}/src/dbc)
include_directories(${PROJECT_NAME} ${CMAKE_SOURCE_DIR}/src/3rd/network)
include_directories(
  ${CMAKE_SOURCE_DIR}/src/3rd/network/rapidjson
  ${CMAKE_SOURCE_DIR}/src/3rd/network/rapidjson/internal
  ${CMAKE_SOURCE_DIR}/src/3rd/network/rapidjson/error
)

add_executable(${PROJECT_NAME} main.cpp chat_message.hpp)

target_link_libraries(${PROJECT_NAME} ${Boost_LIBRARIES} -pthread -ldl -lz)
