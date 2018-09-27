
message(STATUS "module test")


SET(SRC_PATH ${CMAKE_SOURCE_DIR}/src)

#include directory
INCLUDE_DIRECTORIES(
        ${SRC_PATH}/3rd/crypto/secp256k1/include
        ${SRC_PATH}/3rd/db/
        ${SRC_PATH}/3rd/network/libevent
        ${SRC_PATH}/3rd/network/libevent/event2
        ${SRC_PATH}/3rd/network/openssl/internal
        ${SRC_PATH}/3rd/network/openssl/openssl
        ${SRC_PATH}/3rd/network/openssl
        ${SRC_PATH}/3rd/network/rapidjson
        ${SRC_PATH}/3rd/network/rapidjson/internal
        ${SRC_PATH}/3rd/network/rapidjson/error
        ${SRC_PATH}/3rd/snappy
        ${SRC_PATH}/core
        ${SRC_PATH}/
)


# all sub folders under core/
set(CORE_FOLDER_LIST alarm codec common conf conf/validataor
        crypto container env
        initiator log math memory module mutex
        network service_bus service_module
        service_module/pc storage timer_service filter)

foreach(f ${CORE_FOLDER_LIST})
    INCLUDE_DIRECTORIES(  ${SRC_PATH}/core/${f})
endforeach()

message(STATUS "DBC_3RD_LIB_ROOT_PATH ${DBC_3RD_LIB_ROOT_PATH}" )
link_directories(../lib)
link_directories(${DBC_3RD_LIB_ROOT_PATH}/secp256k1)
link_directories(${DBC_3RD_LIB_ROOT_PATH}/openssl)
link_directories(${DBC_3RD_LIB_ROOT_PATH}/libevent)
link_directories(${DBC_3RD_LIB_ROOT_PATH}/snappy)

add_executable(dbc_core_test
        test_main.cpp
        os_math_test.cpp
        module_manager_test.cpp
        topic_manager_test.cpp
        rw_lock_test.cpp
        tcp_socket_test.cpp
        byte_buf_test.cpp
        crypto_test.cpp
        log_test.cpp
        filter_test.cpp
        util_test.cpp
        protocol_test.cpp
        codec_test.cpp
        callback_wait_test.cpp
        id_generator_test.cpp
        conf_manager_test.cpp
        container_message_test.cpp
        pc_service_module_test.cpp
        timer_service_test.cpp
        ip_validator_test.cpp
        port_validator_test.cpp
        matrix_compress_test.cpp
        )

set(dbclib  ${LIBRARY_OUTPUT_PATH}/libdbc_core.a)

if (APPLE)
    TARGET_LINK_LIBRARIES(dbc_core_test ${dbclib} ${Boost_LIBRARIES} -lcrypto -lssl -lsecp256k1 -lgmp -lsnappy)
elseif (LINUX)
    TARGET_LINK_LIBRARIES(dbc_core_test ${dbclib} ${Boost_LIBRARIES} -lboost_thread -lcrypto -lssl -lsecp256k1 -lsnappy -pthread -ldl)
endif()