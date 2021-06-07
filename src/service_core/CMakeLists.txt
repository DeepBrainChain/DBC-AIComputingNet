SET(SRC_PATH ${CMAKE_SOURCE_DIR}/src)

include_directories(
        ${SRC_PATH}/core
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
        ${SRC_PATH}/core/filter
        ${SRC_PATH}/3rd
        ${SRC_PATH}/3rd/crypto/secp256k1/include
        ${SRC_PATH}/3rd/crypto/secp256k1/contrib
        ${SRC_PATH}/3rd/db
        ${SRC_PATH}/3rd/network/libevent
        ${SRC_PATH}/3rd/network/libevent/event2
        ${SRC_PATH}/3rd/network/openssl/internal
        ${SRC_PATH}/3rd/network/openssl/openssl
        ${SRC_PATH}/3rd/network/openssl
        ${SRC_PATH}/3rd/network/rapidjson
        ${SRC_PATH}/3rd/network/rapidjson/internal
        ${SRC_PATH}/3rd/network/rapidjson/error
        ${SRC_PATH}/3rd/snappy
)

set(service_core_src_dir common network p2pnet)

foreach (sub_dir ${service_core_src_dir})
    AUX_SOURCE_DIRECTORY(${SRC_PATH}/service_core/${sub_dir} service_core_src_files)
    INCLUDE_DIRECTORIES(${SRC_PATH}/service_core/${sub_dir})
endforeach ()

add_library(dbc_service_core STATIC ${service_core_src_files})
