cmake_minimum_required(VERSION 3.10)
project(db_show)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_COMPILER "/usr/bin/g++")
set(CMAKE_EXE_LINKER_FLAGS  -no-pie)
set(EXECUTABLE_OUTPUT_PATH ../output)
include_directories(src)
include_directories(src/db/)
link_directories(lib)
add_executable(db_show
        src/ai_db_types.cpp
        src/ai_db_types.h
        src/byte_buf.cpp
        src/byte_buf.h
        src/comm_def.h
        src/common.cpp
        src/db_show.cpp
        src/parse_peer_db.cpp
        src/parse_pro_db.cpp
        src/parse_req_db.cpp
        src/peers_db_types.cpp
        src/peers_db_types.h
        src/protocol.cpp
        src/protocol.h
        src/TToString.h)

target_link_libraries(db_show -lleveldb -pthread)