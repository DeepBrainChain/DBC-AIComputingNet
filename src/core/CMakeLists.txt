
#output setting
SET(OUTPUT_NAME dbc)

#path setting
SET(HOME_PATH .)
SET(SRC_PATH ../../src)

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
)


# all sub folders under core/
set(SRC_FOLDER_LIST alarm codec common conf conf/validataor
        crypto container data_structure env
        initiator log math memory module monitor mutext
        network sem service_bus service_module
        service_module/pc storage timer_service)

set(SRC_LIST)

foreach(f ${SRC_FOLDER_LIST})
#    message(STATUS ${f})
    AUX_SOURCE_DIRECTORY(${SRC_PATH}/core/${f} tmp)
    list(APPEND SRC_LIST ${tmp})
endforeach()


############################################################################

# remove test files from dbc build
#list(REMOVE_ITEM CORE_MATH_SRCS ${SRC_PATH}/core/math/os_math_test.cpp)

SET(LIBRARY_OUTPUT_PATH ${CMAKE_CURRENT_SOURCE_DIR}/lib)


add_library(dbc_core STATIC
        ${SRC_LIST}
        ./network/socket_channel_handler.h
        )
############################################################################

#install(TARGETS dbc_core DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/lib)