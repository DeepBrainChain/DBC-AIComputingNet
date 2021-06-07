SET(HOME_PATH ${CMAKE_SOURCE_DIR})
SET(SRC_PATH ${CMAKE_SOURCE_DIR}/src)

INCLUDE_DIRECTORIES(
        ${SRC_PATH}/core
        ${SRC_PATH}/3rd/crypto/secp256k1/include
        ${SRC_PATH}/3rd/crypto/secp256k1/contrib
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
        ${SRC_PATH}/3rd
        ${SRC_PATH}/3rd/xml
)

set(core_src_dir codec common conf conf/validataor
        crypto crypto/ctaes container oss oss/bill oss/idle_task env
        initiator log math memory module monitor mutex
        network service_bus service_module vm
        service_module/pc storage timer_service filter compress)

foreach(sub_dir ${core_src_dir})
    AUX_SOURCE_DIRECTORY(${SRC_PATH}/core/${sub_dir} core_src_files)
    INCLUDE_DIRECTORIES(${SRC_PATH}/core/${sub_dir})
endforeach()

add_library(dbc_core STATIC ${core_src_files})
