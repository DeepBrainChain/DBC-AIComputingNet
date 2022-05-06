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
        3rd
        3rd/crypto/secp256k1/include
        3rd/crypto/secp256k1/contrib
        3rd/db
        3rd/network
        3rd/network/libevent
        3rd/network/libevent/event2
        3rd/network/rapidjson
        3rd/network/rapidjson/internal
        3rd/network/rapidjson/error
        3rd/cli
        3rd/snappy
        3rd/xml
        3rd/substrate
        3rd/sr25519crust
        3rd/tweetnacl
)

set(service_src_dir
        dbc
        dbc/common
        dbc/util
        dbc/util/utils
        dbc/util/validator
        dbc/util/memory
        dbc/util/math
        dbc/util/filter
        dbc/util/crypto
        dbc/util/crypto/ctaes
        dbc/config
        dbc/db/db_types
        dbc/db
        dbc/network/utils
        dbc/network/compress
        dbc/network/protocol
        dbc/network/channel
        dbc/network/http
        dbc/network
        dbc/log
        dbc/timer
        dbc/service_module
        dbc/server
        dbc/message
        dbc/message/protocol_coder
        dbc/task/vm
        dbc/task/docker
        dbc/task/detail
        dbc/task/detal/disk
        dbc/task/detal/gpu
        dbc/task/detal/image
        dbc/task/detal/info
        dbc/task/detal/iptable
        dbc/task/detal/wallet_rent_task
        dbc/task/detal/wallet_session_id
        dbc/task
        dbc/service_info
        dbc/service/http_request_service
        dbc/service/node_monitor_service
        dbc/service/node_request_service
        dbc/service/peer_request_service
        dbc/service/peer_request_service/multicast
        dbc/service/peer_request_service/socket_channel_handler
        dbc/service
)

foreach(sub_dir ${service_src_dir})
    aux_source_directory(${sub_dir} dbc_src_files)
endforeach()

link_directories(${3RD_LIB_PATH}/secp256k1)
link_directories(${3RD_LIB_PATH}/leveldb)
link_directories(${3RD_LIB_PATH}/libevent)
link_directories(${3RD_LIB_PATH}/readline)
link_directories(${3RD_LIB_PATH}/snappy)
link_directories(${3RD_LIB_PATH}/tinyxml2)
link_directories(${3RD_LIB_PATH}/substrate)
link_directories(${3RD_LIB_PATH}/sr25519)
link_directories(${3RD_LIB_PATH}/tweetnacl)

add_executable(dbc ./main.cpp ${dbc_src_files})

if (LINUX)
    TARGET_LINK_LIBRARIES(dbc ${Boost_LIBRARIES} -lcpp_substrate -lsr25519crust -ltweetnacl -lcrypt -levent
            -lleveldb -lvirt -lvirt-qemu -ltinyxml2 -luuid -levent_core -levent_openssl -levent_pthreads -lcrypto -lssl
            -lsecp256k1 -lreadline -lhistory libncurses.a -lsnappy -pie -pthread -ldl -lz)
endif ()

## check_env ##
set(check_env_src_dir
        check_env
        check_env/util
        )

foreach(sub_dir ${check_env_src_dir})
    aux_source_directory(${sub_dir} check_env_src_files)
endforeach()

add_executable(check_env ${check_env_src_files})

target_link_libraries(check_env ${Boost_LIBRARIES} -lsr25519crust -lcrypt -levent -lleveldb -lvirt
        -ltinyxml2 -luuid -levent_core -levent_openssl -levent_pthreads -lcrypto -lssl -lsecp256k1
        -lreadline -lhistory libncurses.a -lsnappy -pie -pthread -ldl -lz)

## sign_tool ##
aux_source_directory(dbc/common sign_tool_src_files)
aux_source_directory(dbc/util sign_tool_src_files)
aux_source_directory(dbc/util/utils sign_tool_src_files)
aux_source_directory(dbc/util/validator sign_tool_src_files)
aux_source_directory(dbc/util/memory sign_tool_src_files)
aux_source_directory(dbc/util/math sign_tool_src_files)
aux_source_directory(dbc/util/filter sign_tool_src_files)
aux_source_directory(dbc/util/crypto sign_tool_src_files)
aux_source_directory(dbc/util/crypto/ctaes sign_tool_src_files)

add_executable(sign_tool ./sign_tool/main.cpp ${sign_tool_src_files})

target_link_directories(sign_tool PRIVATE ${3RD_LIB_PATH}/openssl)

target_link_libraries(sign_tool ${Boost_LIBRARIES} -lcpp_substrate -lsr25519crust -ltweetnacl
        -luuid -lcrypto -lssl -lsecp256k1 -pthread -ldl -lz -static-libstdc++)
