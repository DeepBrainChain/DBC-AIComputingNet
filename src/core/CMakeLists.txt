
#output setting
SET(OUTPUT_NAME dbc)

#path setting
SET(HOME_PATH ${CMAKE_SOURCE_DIR})
SET(SRC_PATH ${CMAKE_SOURCE_DIR}/src)

#include directory
INCLUDE_DIRECTORIES(
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
        ${SRC_PATH}/core
)


# all sub folders under core/
set(SRC_FOLDER_LIST codec common conf conf/validataor
        crypto container oss oss/bill oss/idle_task env
        initiator log math memory module monitor mutex
        network service_bus service_module
        service_module/pc storage timer_service filter compress)

set(SRC_LIST)

foreach(f ${SRC_FOLDER_LIST})
    message(STATUS ${f})
    AUX_SOURCE_DIRECTORY(${SRC_PATH}/core/${f} tmp)
    list(APPEND SRC_LIST ${tmp})

    INCLUDE_DIRECTORIES(  ${SRC_PATH}/core/${f})

endforeach()


############################################################################

# remove test files from dbc build
#list(REMOVE_ITEM CORE_MATH_SRCS ${SRC_PATH}/core/math/os_math_test.cpp)

add_library(dbc_core STATIC
        ${SRC_LIST}
        ./network/socket_channel_handler.h
        )
############################################################################

#install(TARGETS dbc_core DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/lib)