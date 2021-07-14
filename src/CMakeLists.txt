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
        ./3rd
        ./3rd/crypto/secp256k1/include
        ./3rd/crypto/secp256k1/contrib
        ./3rd/db
        ./3rd/network
        ./3rd/network/libevent
        ./3rd/network/libevent/event2
        ./3rd/network/openssl
        ./3rd/network/openssl/internal
        ./3rd/network/openssl/openssl
        ./3rd/network/rapidjson
        ./3rd/network/rapidjson/internal
        ./3rd/network/rapidjson/error
        ./3rd/cli
        ./3rd/snappy
        ./3rd/xml
        ./3rd/substrate
        ./3rd/sr25519crust
)

#core
set(core_src_dir
        core
        core/common
        core/conf
        core/conf/validator
        core/crypto
        core/crypto/ctaes
        core/container
        core/oss
        core/oss/bill
        core/oss/idle_task
        core/env
        core/initiator
        core/log
        core/math
        core/memory
        core/module
        core/monitor
        core/mutex
        core/network
        core/network/compress
        core/network/protocol
        core/service_bus
        core/vm
        core/service_module
        core/storage
        core/timer_service
        core/filter)

foreach(sub_dir ${core_src_dir})
    include_directories(dbc/${sub_dir})
endforeach()
foreach(sub_dir ${core_src_dir})
    aux_source_directory(dbc/${sub_dir} dbc_src_files)
endforeach()

# service
set(service_src_dir . common task node_info
        service
        service/message
        service/message/protocol_coder
        service/cmd_request_service
        service/http_request_service
        service/peer_request_service
        service/peer_request_service/socket_channel_handler
        service/node_request_service
        service/node_request_service/task_schedule
        service/node_request_service/task_schedule/db
        service/node_request_service/task_schedule/vm
        service/node_request_service/task_schedule/resource)

foreach(sub_dir ${service_src_dir})
    include_directories(dbc/${sub_dir})
endforeach()
foreach(sub_dir ${service_src_dir})
    aux_source_directory(dbc/${sub_dir} dbc_src_files)
endforeach()

include_directories(.)
aux_source_directory(. dbc_src_files)

link_directories(${3RD_LIB_PATH}/secp256k1)
link_directories(${3RD_LIB_PATH}/openssl)
link_directories(${3RD_LIB_PATH}/leveldb)
link_directories(${3RD_LIB_PATH}/libevent)
link_directories(${3RD_LIB_PATH}/readline)
link_directories(${3RD_LIB_PATH}/snappy)
link_directories(${3RD_LIB_PATH}/tinyxml2)
link_directories(${3RD_LIB_PATH}/substrate)
link_directories(${3RD_LIB_PATH}/sr25519)

add_executable(dbc ${dbc_src_files})

if (LINUX)
    TARGET_LINK_LIBRARIES(dbc ${Boost_LIBRARIES} -lsc_cli -lsr25519crust -lcrypt -levent -lleveldb -lvirt -ltinyxml2 -luuid -levent_core -levent_openssl -levent_pthreads -lcrypto -lssl -lsecp256k1 -lreadline -lhistory libncurses.a -lsnappy -pie -pthread -ldl -lz)
endif ()

if (APPLE)
    TARGET_LINK_LIBRARIES(dbc ${Boost_LIBRARIES} -lcrypt -levent -lleveldb -lvirt -ltinyxml2 -luuid -levent_core -levent_openssl -levent_pthreads -lcrypto -lssl -lsecp256k1 libedit.a libncurses.a -ldl -lz)
    TARGET_LINK_LIBRARIES(dbc -lsnappy -lgmp)
endif ()

###########################
## test
###########################
#add_subdirectory(unittest/tests/test_keypair)

