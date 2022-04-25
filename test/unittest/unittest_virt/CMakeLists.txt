cmake_minimum_required (VERSION 3.2)
project(virt_test)

set(CMAKE_CXX_STANDARD 11)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/output)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/output)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/output)

link_directories(${3RD_LIB_PATH}/tinyxml2)

# include_directories(${CMAKE_SOURCE_DIR}/src/3rd/substrate)
include_directories(${CMAKE_SOURCE_DIR}/src/3rd/xml)

# virt_monitor_test
add_executable(virt_monitor_test virt_monitor.cpp vir_helper.cpp)
target_link_libraries(virt_monitor_test
${Boost_LIBRARIES}
        -pthread -ldl -lz -ltinyxml2 -lvirt -lvirt-qemu)

# virt_snapshot_test
add_executable(virt_snapshot_test virt_snapshot.cpp vir_helper.cpp)
target_link_libraries(virt_snapshot_test
${Boost_LIBRARIES}
        -pthread -ldl -lz -ltinyxml2 -lvirt -lvirt-qemu)

# virt_nwfilter_test
add_executable(virt_nwfilter_test virt_nwfilter.cpp vir_helper.cpp)
target_link_libraries(virt_nwfilter_test
${Boost_LIBRARIES}
        -pthread -ldl -lz -ltinyxml2 -lvirt -lvirt-qemu -luuid)
