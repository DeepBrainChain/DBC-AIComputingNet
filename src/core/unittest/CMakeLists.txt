
message(STATUS "module test")


SET(SRC_PATH ../../../src)

include_directories(
        ${SRC_PATH}/core/common
        ${SRC_PATH}/core/data_structure
        ${SRC_PATH}/core/log
        ${SRC_PATH}/core/timer_service
        ${SRC_PATH}/core/service_bus
        ${SRC_PATH}/core/module
        ${SRC_PATH}/core/math
        ${SRC_PATH}/core/mutex
        ${SRC_PATH}/core/memory
        ${SRC_PATH}/core/crypto
        ${SRC_PATH}
)

message(STATUS "DBC_3RD_LIB_ROOT_PATH ${DBC_3RD_LIB_ROOT_PATH}" )
link_directories(../lib)
link_directories(${DBC_3RD_LIB_ROOT_PATH}/secp256k1)
link_directories(${DBC_3RD_LIB_ROOT_PATH}/openssl)

add_executable(dbc_core_test
        test_main.cpp
        os_math_test.cpp
        module_manager_test.cpp
        topic_manager_test.cpp
        rw_lock_test.cpp
        tcp_socket_test.cpp
        byte_buf_test.cpp
        crypto_test.cpp
        )


if (APPLE)
    TARGET_LINK_LIBRARIES(dbc_core_test libdbc_core.a ${Boost_LIBRARIES} -lcrypto -lssl -lsecp256k1)
elseif (LINUX)
    TARGET_LINK_LIBRARIES(dbc_core_test libdbc_core.a ${Boost_LIBRARIES} -lboost_thread.a -lcrypto -lssl -lsecp256k1 -pthread -ldl)
endif()