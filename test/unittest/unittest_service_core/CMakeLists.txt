
message(STATUS "service code unit test")

SET(SRC_PATH ${CMAKE_SOURCE_DIR}/src)

include_directories(
        ${SRC_PATH}/core/codec
        ${SRC_PATH}/core/common
        ${SRC_PATH}/core/conf
        ${SRC_PATH}/core/conf/validator
        ${SRC_PATH}/core/container
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
        ${SRC_PATH}/core/service_module/pc
        ${SRC_PATH}/core/storage
        ${SRC_PATH}/core/timer_service
        ${SRC_PATH}/core
        ${SRC_PATH}/

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

        ${SRC_PATH}/service_core/codec
        ${SRC_PATH}/service_core/codec/thrift
        ${SRC_PATH}/service_core

)

link_directories(${SRC_PATH}/core/lib)
link_directories(${DBC_3RD_LIB_ROOT_PATH}/secp256k1)
link_directories(${DBC_3RD_LIB_ROOT_PATH}/openssl)
link_directories(${DBC_3RD_LIB_ROOT_PATH}/snappy)
link_directories(${DBC_3RD_LIB_ROOT_PATH}/libevent)

add_executable(dbc_service_core_test
        test_main.cpp
        matrix_codec_test.cpp
        ai_crypto_test.cpp)

set(dbclib ${LIBRARY_OUTPUT_PATH}/libdbc_service_core.a ${LIBRARY_OUTPUT_PATH}/libdbc_core.a)

if(APPLE)
    TARGET_LINK_LIBRARIES(dbc_service_core_test ${dbclib} ${Boost_LIBRARIES} -levent -levent_core -levent_openssl -levent_pthreads -lcrypto -lssl -lsecp256k1 -lgmp -lsnappy)
elseif(LINUX)
    TARGET_LINK_LIBRARIES(dbc_service_core_test ${dbclib} ${Boost_LIBRARIES} -levent -levent_core -levent_openssl -levent_pthreads -lcrypto -lssl -lsecp256k1 -lsnappy -lboost_thread -pthread -ldl)
endif()