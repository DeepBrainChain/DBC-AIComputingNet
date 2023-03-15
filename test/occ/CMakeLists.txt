cmake_minimum_required (VERSION 3.2)
project(occ_test)

set(CMAKE_CXX_FLAGS "-fPIC -w -DEVENT__HAVE_OPENSSL")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS} -g")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS} -O2")

set(CMAKE_CXX_STANDARD 11)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/output)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/output)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/output)

# include_directories(${CMAKE_SOURCE_DIR}/src/dbc)
include_directories(${PROJECT_NAME}
    ${CMAKE_SOURCE_DIR}/src/dbc
    ${CMAKE_SOURCE_DIR}/src/dbc/network/protocol
    ${CMAKE_SOURCE_DIR}/src/dbc/util
    ${CMAKE_SOURCE_DIR}/src/dbc/util/memory
    ${CMAKE_SOURCE_DIR}/src/3rd
    ${CMAKE_SOURCE_DIR}/src/3rd/network
    ${CMAKE_SOURCE_DIR}/src/3rd/network/libevent
    ${CMAKE_SOURCE_DIR}/src/3rd/network/libevent/event2
    ${CMAKE_SOURCE_DIR}/src/3rd/crypto/secp256k1/include
    ${CMAKE_SOURCE_DIR}/src/3rd/crypto/secp256k1/contrib
    ${CMAKE_SOURCE_DIR}/src/3rd/substrate
    ${CMAKE_SOURCE_DIR}/src/3rd/tweetnacl)

aux_source_directory(${CMAKE_SOURCE_DIR}/src/dbc/message dbc_src_files)
aux_source_directory(${CMAKE_SOURCE_DIR}/src/dbc/network/protocol dbc_src_files)
aux_source_directory(${CMAKE_SOURCE_DIR}/src/dbc/util dbc_src_files)
aux_source_directory(${CMAKE_SOURCE_DIR}/src/dbc/util/utils dbc_src_files)
aux_source_directory(${CMAKE_SOURCE_DIR}/src/dbc/util/validator dbc_src_files)
aux_source_directory(${CMAKE_SOURCE_DIR}/src/dbc/util/memory dbc_src_files)
aux_source_directory(${CMAKE_SOURCE_DIR}/src/dbc/util/math dbc_src_files)
aux_source_directory(${CMAKE_SOURCE_DIR}/src/dbc/util/filter dbc_src_files)
aux_source_directory(${CMAKE_SOURCE_DIR}/src/dbc/util/crypto dbc_src_files)
aux_source_directory(${CMAKE_SOURCE_DIR}/src/dbc/util/crypto/ctaes dbc_src_files)

link_directories(${3RD_LIB_PATH}/libevent)
link_directories(${3RD_LIB_PATH}/secp256k1)
link_directories(${3RD_LIB_PATH}/substrate)
link_directories(${3RD_LIB_PATH}/sr25519)
link_directories(${3RD_LIB_PATH}/tinyxml2)
link_directories(${3RD_LIB_PATH}/tweetnacl)

add_executable(${PROJECT_NAME} main.cpp Preset.cpp ${dbc_src_files})

target_link_libraries(${PROJECT_NAME} ${Boost_LIBRARIES} -lcpp_substrate -lsr25519crust
    -ltweetnacl -lcrypt -levent
    -levent_core -levent_openssl -levent_pthreads -luuid -lcrypto -lssl -lsecp256k1 
    -pthread -ldl -lz -ltinyxml2)
