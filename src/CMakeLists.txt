cmake_minimum_required (VERSION 3.2)
project("dbc")

set(CMAKE_CXX_FLAGS "-fPIC -w -DEVENT__HAVE_OPENSSL")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS} -g")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS} -O2")

set(CMAKE_CXX_STANDARD 11)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/output)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/output)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/output)

include_directories(
        dbc
        ./3rd
        ./3rd/crypto/secp256k1/include
        ./3rd/crypto/secp256k1/contrib
        ./3rd/db
        ./3rd/network
        ./3rd/network/libevent
        ./3rd/network/libevent/event2
        #./3rd/network/openssl
        #./3rd/network/openssl/internal
        #./3rd/network/openssl/openssl
        ./3rd/network/rapidjson
        ./3rd/network/rapidjson/internal
        ./3rd/network/rapidjson/error
        ./3rd/cli
        ./3rd/snappy
        ./3rd/xml
        ./3rd/substrate
        ./3rd/sr25519crust
        ./3rd/network/websocket
        ./3rd/network/websocket/ixwebsocket
)

set(service_src_dir
        .
        dbc
        dbc/common
        dbc/common/def
        dbc/util
        dbc/util/utils
        dbc/util/validator
        dbc/util/memory
        dbc/util/math
        dbc/util/filter
        dbc/util/crypto
        dbc/util/crypto/ctaes
        dbc/config
        dbc/data
        dbc/data/node_info
        dbc/data/resource
        dbc/data/session_id
        dbc/data/task
        dbc/data/task/db
        dbc/data/task/vm
        dbc/data/task/container
        dbc/network
        dbc/network/compress
        dbc/network/protocol
        dbc/network/socket_channel_handler
        dbc/log
        dbc/timer
        dbc/module
        dbc/service_module
        dbc/server
        dbc/service
        dbc/service/message
        dbc/service/message/protocol_coder
        dbc/service/oss
        dbc/service/oss/bill
        dbc/service/oss/idle_task
        dbc/service/common_service
        dbc/service/http_request_service
        dbc/service/cmd_request_service
        dbc/service/node_request_service
        dbc/service/peer_request_service
)

foreach(sub_dir ${service_src_dir})
    aux_source_directory(${sub_dir} dbc_src_files)
endforeach()

link_directories(${3RD_LIB_PATH}/secp256k1)
#link_directories(${3RD_LIB_PATH}/openssl)
link_directories(${3RD_LIB_PATH}/leveldb)
link_directories(${3RD_LIB_PATH}/libevent)
link_directories(${3RD_LIB_PATH}/readline)
link_directories(${3RD_LIB_PATH}/snappy)
link_directories(${3RD_LIB_PATH}/tinyxml2)
link_directories(${3RD_LIB_PATH}/substrate)
link_directories(${3RD_LIB_PATH}/sr25519)
link_directories(${3RD_LIB_PATH}/websocket)

add_executable(dbc ${dbc_src_files})

if (LINUX)
    TARGET_LINK_LIBRARIES(dbc ${Boost_LIBRARIES} -lsc_cli -lixwebsocket -lsr25519crust -lcrypt -levent -lleveldb -lvirt -ltinyxml2 -luuid -levent_core -levent_openssl -levent_pthreads -lcrypto -lssl -lsecp256k1 -lreadline -lhistory libncurses.a -lsnappy -pie -pthread -ldl -lz)
endif ()

if (APPLE)
    TARGET_LINK_LIBRARIES(dbc ${Boost_LIBRARIES} -lcrypt -levent -lleveldb -lvirt -ltinyxml2 -luuid -levent_core -levent_openssl -levent_pthreads -lcrypto -lssl -lsecp256k1 libedit.a libncurses.a -ldl -lz)
    TARGET_LINK_LIBRARIES(dbc -lsnappy -lgmp)
endif ()

###########################
## test
###########################
#add_subdirectory(unittest/tests/test_keypair)

