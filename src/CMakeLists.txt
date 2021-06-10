message(STATUS "begin to cmake.........")

SET(HOME_PATH ${CMAKE_SOURCE_DIR})
SET(SRC_PATH ${CMAKE_SOURCE_DIR}/src)

SET(EXECUTABLE_OUTPUT_PATH ${HOME_PATH}/output/)
SET(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${HOME_PATH}/output)
SET(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${HOME_PATH}/output)
SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${HOME_PATH}/output)

INCLUDE_DIRECTORIES(
        ${SRC_PATH}
        ${SRC_PATH}/dbc
        ${SRC_PATH}/dbc/common
        ${SRC_PATH}/dbc/task
        ${SRC_PATH}/dbc/db
        ${SRC_PATH}/dbc/network
        ${SRC_PATH}/dbc/network/p2pnet
        ${SRC_PATH}/dbc/service
        ${SRC_PATH}/dbc/service/message
        ${SRC_PATH}/dbc/service/cmd_line_service
        ${SRC_PATH}/dbc/service/cmd_request_service
        ${SRC_PATH}/dbc/service/http_request_service
        ${SRC_PATH}/dbc/service/node_request_service
        ${SRC_PATH}/dbc/service/node_request_service/task_schedule
        ${SRC_PATH}/dbc/service/node_info_exchange
        ${SRC_PATH}/core
        ${SRC_PATH}/core/codec
        ${SRC_PATH}/core/common
        ${SRC_PATH}/core/conf
        ${SRC_PATH}/core/conf/validator
        ${SRC_PATH}/core/container
        ${SRC_PATH}/core/oss
        ${SRC_PATH}/core/crypto
        ${SRC_PATH}/core/env
        ${SRC_PATH}/core/initiator
        ${SRC_PATH}/core/log
        ${SRC_PATH}/core/math
        ${SRC_PATH}/core/memory
        ${SRC_PATH}/core/module
        ${SRC_PATH}/core/mutex
        ${SRC_PATH}/core/network
        ${SRC_PATH}/core/service_bus
        ${SRC_PATH}/core/service_module
        ${SRC_PATH}/core/storage
        ${SRC_PATH}/core/timer_service
        ${SRC_PATH}/core/vm
        ${SRC_PATH}/core/filter
        ${SRC_PATH}/3rd
        ${SRC_PATH}/3rd/crypto/secp256k1/include
        ${SRC_PATH}/3rd/crypto/secp256k1/contrib
        ${SRC_PATH}/3rd/db
        ${SRC_PATH}/3rd/network/libevent
        ${SRC_PATH}/3rd/network/libevent/event2
        ${SRC_PATH}/3rd/network/openssl
        ${SRC_PATH}/3rd/network/openssl/internal
        ${SRC_PATH}/3rd/network/openssl/openssl
        ${SRC_PATH}/3rd/network/rapidjson
        ${SRC_PATH}/3rd/network/rapidjson/internal
        ${SRC_PATH}/3rd/network/rapidjson/error
        ${SRC_PATH}/3rd/cli
        ${SRC_PATH}/3rd/snappy
)

aux_source_directory(${SRC_PATH} dbc_src_files)
aux_source_directory(${SRC_PATH}/dbc dbc_src_files)
aux_source_directory(${SRC_PATH}/dbc/common dbc_src_files)
aux_source_directory(${SRC_PATH}/dbc/task dbc_src_files)
aux_source_directory(${SRC_PATH}/dbc/db dbc_src_files)
aux_source_directory(${SRC_PATH}/dbc/network dbc_src_files)
aux_source_directory(${SRC_PATH}/dbc/network/p2pnet dbc_src_files)
aux_source_directory(${SRC_PATH}/dbc/service dbc_src_files)
aux_source_directory(${SRC_PATH}/dbc/service/message dbc_src_files)
aux_source_directory(${SRC_PATH}/dbc/service/cmd_line_service dbc_src_files)
aux_source_directory(${SRC_PATH}/dbc/service/cmd_request_service dbc_src_files)
aux_source_directory(${SRC_PATH}/dbc/service/http_request_service dbc_src_files)
aux_source_directory(${SRC_PATH}/dbc/service/node_request_service dbc_src_files)
aux_source_directory(${SRC_PATH}/dbc/service/node_request_service/task_schedule dbc_src_files)
aux_source_directory(${SRC_PATH}/dbc/service/node_info_exchange dbc_src_files)

link_directories(${DBC_3RD_LIB_ROOT_PATH}/secp256k1)
link_directories(${DBC_3RD_LIB_ROOT_PATH}/openssl)
link_directories(${DBC_3RD_LIB_ROOT_PATH}/leveldb)
link_directories(${DBC_3RD_LIB_ROOT_PATH}/libevent)
link_directories(${DBC_3RD_LIB_ROOT_PATH}/readline)
link_directories(${DBC_3RD_LIB_ROOT_PATH}/snappy)
link_directories(${DBC_3RD_LIB_ROOT_PATH}/tinyxml2)

add_executable(dbc ${dbc_src_files})

set(dbclib ${LIBRARY_OUTPUT_PATH}/libdbc_core.a)

if (LINUX)
    TARGET_LINK_LIBRARIES(dbc ${Boost_LIBRARIES} ${dbclib} -lcrypt -levent -lleveldb -lvirt -ltinyxml2 -luuid -levent_core -levent_openssl -levent_pthreads -lcrypto -lssl -lsecp256k1 -lreadline -lhistory libncurses.a -lsnappy -pie -pthread -ldl)
endif ()

if (APPLE)
    TARGET_LINK_LIBRARIES(dbc ${Boost_LIBRARIES} ${dbclib} -lcrypt -levent -lleveldb -lvirt -ltinyxml2 -luuid -levent_core -levent_openssl -levent_pthreads -lcrypto -lssl -lsecp256k1 libedit.a libncurses.a -ldl)
    TARGET_LINK_LIBRARIES(dbc -lsnappy -lgmp)
endif ()

MESSAGE(STATUS "end to cmake.........")
