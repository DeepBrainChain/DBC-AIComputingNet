cmake_minimum_required (VERSION 3.2)
project(zabbix_sender_test)

set(CMAKE_CXX_STANDARD 11)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/output)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/output)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/output)

# include_directories(${CMAKE_SOURCE_DIR}/src/3rd/substrate)
include_directories(${CMAKE_SOURCE_DIR}/src/dbc)
include_directories(${PROJECT_NAME} ${CMAKE_SOURCE_DIR}/src/3rd/xml ${CMAKE_SOURCE_DIR}/src/3rd/network)

link_directories(${3RD_LIB_PATH}/tinyxml2)

add_executable(${PROJECT_NAME} main.cpp TaskMonitorInfo.cpp virImpl.cpp zabbixSender.cpp)

target_link_libraries(${PROJECT_NAME} ${Boost_LIBRARIES} -pthread -ldl -lz -ltinyxml2 -lvirt)
