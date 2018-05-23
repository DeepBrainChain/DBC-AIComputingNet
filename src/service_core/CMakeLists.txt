
SET(SRC_PATH ${CMAKE_SOURCE_DIR}/src)

#include directory
INCLUDE_DIRECTORIES(
        ${SRC_PATH}/core/alarm
        ${SRC_PATH}/core/codec
        ${SRC_PATH}/core/common
        ${SRC_PATH}/core/conf
        ${SRC_PATH}/core/conf/validator
        ${SRC_PATH}/core/container
        ${SRC_PATH}/core/crypto
        ${SRC_PATH}/core/data_structure
        ${SRC_PATH}/core/env
        ${SRC_PATH}/core/initiator
        ${SRC_PATH}/core/log
        ${SRC_PATH}/core/math
        ${SRC_PATH}/core/memory
        ${SRC_PATH}/core/module
        ${SRC_PATH}/core/monitor
        ${SRC_PATH}/core/mutex
        ${SRC_PATH}/core/network
        ${SRC_PATH}/core/sem
        ${SRC_PATH}/core/service_bus
        ${SRC_PATH}/core/service_module
        ${SRC_PATH}/core/service_module/pc
        ${SRC_PATH}/core/storage
        ${SRC_PATH}/core/timer_service

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

        ${SRC_PATH}/service_core/codec
        ${SRC_PATH}/service_core/codec/thrift
        ${SRC_PATH}/service_core/common
        ${SRC_PATH}/service_core/network
        ${SRC_PATH}/service_core/p2pnet
)


set(SRC_FOLDER_LIST codec codec/thrift common network p2pnet)
set(SRC_LIST)

foreach(f ${SRC_FOLDER_LIST})
#    message(STATUS ${f})
    AUX_SOURCE_DIRECTORY(${SRC_PATH}/service_core/${f} tmp)
    list(APPEND SRC_LIST ${tmp})
endforeach()


############################################################################
SET(LIBRARY_OUTPUT_PATH ${CMAKE_CURRENT_SOURCE_DIR}/lib)

add_library(dbc_service_core STATIC
        ${SRC_LIST}
        )
############################################################################

SET(LIBRARIES)
