############################################################################
#
# Copyright (c) 2017-2018 DeepBrainChain core team All rights reserved.
#
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php
#
############################################################################

message(STATUS "begin to cmake.........")


############################################################################

SET(HOME_PATH ${CMAKE_SOURCE_DIR})
SET(SRC_PATH ${CMAKE_SOURCE_DIR}/src)


############################################################################

if (LINUX)
SET(EXECUTABLE_OUTPUT_PATH ${HOME_PATH}/output/)
SET(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${HOME_PATH}/output)
SET(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${HOME_PATH}/output)
SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${HOME_PATH}/output)
endif()


#include directory
INCLUDE_DIRECTORIES(
${SRC_PATH}

${SRC_PATH}/api 
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

${SRC_PATH}/service_core/codec 
${SRC_PATH}/service_core/codec/thrift
${SRC_PATH}/service_core/common
${SRC_PATH}/service_core/network
${SRC_PATH}/service_core/p2pnet 

${SRC_PATH}/service/ai/deep_brain_chain/conf 
${SRC_PATH}/service/ai/deep_brain_chain/initiator
${SRC_PATH}/service/ai/deep_brain_chain/ai_training

${SRC_PATH}/3rd/crypto/secp256k1/include
${SRC_PATH}/3rd/db/
${SRC_PATH}/3rd/network/libevent
${SRC_PATH}/3rd/network/libevent/event2
${SRC_PATH}/3rd/network/openssl/internal
${SRC_PATH}/3rd/network/openssl/openssl
${SRC_PATH}/3rd/network/rapidjson
${SRC_PATH}/3rd/network/rapidjson/internal
${SRC_PATH}/3rd/network/rapidjson/error

${SRC_PATH}/api
)

#source directory
AUX_SOURCE_DIRECTORY(${SRC_PATH} DIR_SRCS)
AUX_SOURCE_DIRECTORY(${SRC_PATH}/api API_SRCS)

AUX_SOURCE_DIRECTORY(${SRC_PATH}/service/ai/deep_brain_chain/conf SERVICE_DBC_CONF_SRCS)
AUX_SOURCE_DIRECTORY(${SRC_PATH}/service/ai/deep_brain_chain/initiator SERVICE_DBC_INITIATOR_SRCS)
AUX_SOURCE_DIRECTORY(${SRC_PATH}/service/ai/deep_brain_chain/ai_training SERVICE_DBC_TRAINING_SRCS)

AUX_SOURCE_DIRECTORY(${SRC_PATH}/3rd/db/leveldb THIRD_LEVEL_DB_SRCS)
AUX_SOURCE_DIRECTORY(${SRC_PATH}/3rd/network/rapidjson THIRD_RAPIDJSON_SRCS)
AUX_SOURCE_DIRECTORY(${SRC_PATH}/3rd/network/rapidjson/internal THIRD_RAPIDJSON_INTERNAL_SRCS)
AUX_SOURCE_DIRECTORY(${SRC_PATH}/3rd/network/rapidjson/error THIRD_RAPIDJSON_ERROR_SRCS)
AUX_SOURCE_DIRECTORY(${SRC_PATH}/3rd/network/libevent THIRD_LIBEVENT_SRCS)
AUX_SOURCE_DIRECTORY(${SRC_PATH}/3rd/network/libevent/event2 THIRD_LIBEVENT_EVENT2_SRCS)

############################################################################



link_directories(${SRC_PATH}/core/lib)
link_directories(${SRC_PATH}/service_core/lib)
# note: cmake link directories must be set before add_executable()!


message(STATUS "DBC_3RD_LIB_ROOT_PATH ${DBC_3RD_LIB_ROOT_PATH}" )
link_directories(${DBC_3RD_LIB_ROOT_PATH}/secp256k1)
link_directories(${DBC_3RD_LIB_ROOT_PATH}/openssl)
link_directories(${DBC_3RD_LIB_ROOT_PATH}/leveldb)
link_directories(${DBC_3RD_LIB_ROOT_PATH}/libevent)


#add target source
ADD_EXECUTABLE(dbc
${DIR_SRCS} 
${API_SRCS}
${SERVICE_DBC_CONF_SRCS} 
${SERVICE_DBC_INITIATOR_SRCS}
${SERVICE_DBC_TRAINING_SRCS}
${THIRD_LEVEL_DB_SRCS}
${THIRD_RAPIDJSON_SRCS}
${THIRD_RAPIDJSON_INTERNAL_SRCS}
${THIRD_RAPIDJSON_ERROR_SRCS}
${THIRD_LIBEVENT_SRCS}
${THIRD_LIBEVENT_EVENT2_SRCS})

############################################################################

#library 
set(LIBRARIES)

TARGET_LINK_LIBRARIES(dbc ${LIBRARIES} ${Boost_LIBRARIES} libdbc_core.a libdbc_service_core.a -levent -lleveldb -levent_core -levent_openssl -levent_pthreads -lcrypto -lssl -lsecp256k1 -pthread -ldl)

if (APPLE)
    TARGET_LINK_LIBRARIES(dbc -lsnappy)
endif()

############################################################################


MESSAGE(STATUS "end to cmake.........")

