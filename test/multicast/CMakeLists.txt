cmake_minimum_required (VERSION 3.2)
project(multicast_test)

set(CMAKE_CXX_STANDARD 11)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/output)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/output)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/output)

add_executable(${PROJECT_NAME} main.cpp multicast_manager.cpp)
target_link_libraries(${PROJECT_NAME} ${Boost_LIBRARIES} -pthread)

add_executable(msender sender.cpp)
target_link_libraries(msender ${Boost_LIBRARIES} -pthread)

add_executable(mreceiver receiver.cpp)
target_link_libraries(mreceiver ${Boost_LIBRARIES} -pthread)
