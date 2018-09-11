
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
        ${SRC_PATH}/core
        ${SRC_PATH}/
)


# all sub folders under core/
set(CORE_FOLDER_LIST alarm codec common conf conf/validataor
        crypto container data_structure env
        initiator log math memory module monitor mutex
        network sem service_bus service_module
        service_module/pc storage timer_service filter)

foreach(f ${CORE_FOLDER_LIST})
    INCLUDE_DIRECTORIES(  ${SRC_PATH}/core/${f})
endforeach()

message(STATUS "DBC_3RD_LIB_ROOT_PATH ${DBC_3RD_LIB_ROOT_PATH}" )
link_directories(../lib)
link_directories(${DBC_3RD_LIB_ROOT_PATH}/secp256k1)
link_directories(${DBC_3RD_LIB_ROOT_PATH}/openssl)
link_directories(${DBC_3RD_LIB_ROOT_PATH}/libevent)

add_executable(dbc_core_network_test
        test_main.cpp
        connection_manager_test.cpp
        http_client_test.cpp
        )

set(dbclib  ${LIBRARY_OUTPUT_PATH}/libdbc_core.a)

if (APPLE)
    TARGET_LINK_LIBRARIES(dbc_core_network_test ${dbclib} ${Boost_LIBRARIES} -lcrypto -lssl -lsecp256k1 -lgmp  -pthread -ldl -levent -levent_core -levent_openssl -levent_pthreads)
elseif (LINUX)
    TARGET_LINK_LIBRARIES(dbc_core_network_test ${dbclib} ${Boost_LIBRARIES} -lcrypto -lssl -lsecp256k1 -pthread -ldl -levent -levent_core -levent_openssl -levent_pthreads)
endif()

