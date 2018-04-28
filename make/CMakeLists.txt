############################################################################
#
# Copyright (c) 2017-2018 DeepBrainChain core team All rights reserved.
#
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php
#
############################################################################

MESSAGE(STATUS "begin to cmake.........")

############################################################################

#cmake version
CMAKE_MINIMUM_REQUIRED(VERSION 2.8)

#project
PROJECT(DBC)

############################################################################

#debug setting
SET(DEBUG_MODE ON)

#c++ compiler setting
#here is different in APPLE and Linux
SET(CMAKE_CXX_FLAGS "-std=c++11" )
SET(CMAKE_CXX_COMPILER "/usr/bin/g++")

SET(MATRIX_FLAGS "-D__linux__ -D__x86_64__ ")
#MAC -DHAVE_GETENTROPY_RAND

#version setting
SET(VERSION_MAJOR 0)
SET(VERSION_MINOR 2)
SET(VERSION_PATCH 0)
SET(VERSION "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}")

#output setting
SET(OUTPUT_NAME dbc)

#path setting
SET(HOME_PATH ..)
SET(SRC_PATH ../src)
SET(EXECUTABLE_OUTPUT_PATH ${HOME_PATH}/bin)
SET(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${HOME_PATH}/bin)
SET(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${HOME_PATH}/output)
SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${HOME_PATH}/output)
SET(LIB_HOME_PATH ${HOME_PATH}/lib)

# Boost settings
SET(Boost_INCLUDE_DIR　"/usr/local/include/boost")
SET(Boost_LIBRARY_DIR  "/usr/local/lib")

SET(Boost_USE_STATIC_LIBS     ON)
SET(Boost_USE_MULTITHREADED   ON)
SET(Boost_USE_STATIC_RUNTIME  ON)

FIND_PACKAGE(Boost 1.66 REQUIRED COMPONENTS thread date_time system filesystem log log_setup exception program_options serialization signals serialization chrono unit_test_framework context)

############################################################################

#include directory
INCLUDE_DIRECTORIES(
${Boost_INCLUDE_DIR} 

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


############################################################################

#link directory
LINK_DIRECTORIES(${Boost_LIBRARY_DIR})

############################################################################

#ADD_SUBDIRECTORY(${SRC_PATH})

############################################################################

#source directory
AUX_SOURCE_DIRECTORY(${SRC_PATH} DIR_SRCS)

AUX_SOURCE_DIRECTORY(${SRC_PATH}/api API_SRCS)

AUX_SOURCE_DIRECTORY(${SRC_PATH}/core/alarm CORE_ALARM_SRCS)
AUX_SOURCE_DIRECTORY(${SRC_PATH}/core/codec CORE_CODEC_SRCS)
AUX_SOURCE_DIRECTORY(${SRC_PATH}/core/common CORE_COMMON_SRCS)
AUX_SOURCE_DIRECTORY(${SRC_PATH}/core/conf CORE_CONF_SRCS)
AUX_SOURCE_DIRECTORY(${SRC_PATH}/core/conf/validator CORE_CONF_VALIDATOR_SRCS)
AUX_SOURCE_DIRECTORY(${SRC_PATH}/core/crypto CORE_CRYPTO)
AUX_SOURCE_DIRECTORY(${SRC_PATH}/core/container CORE_CONTAINER)
AUX_SOURCE_DIRECTORY(${SRC_PATH}/core/data_structure CORE_DATA_STRU_SRCS)
AUX_SOURCE_DIRECTORY(${SRC_PATH}/core/env CORE_ENV_SRCS)
AUX_SOURCE_DIRECTORY(${SRC_PATH}/core/initiator CORE_INITIATOR_SRCS)
AUX_SOURCE_DIRECTORY(${SRC_PATH}/core/log CORE_LOG_SRCS)
AUX_SOURCE_DIRECTORY(${SRC_PATH}/core/math CORE_MATH_SRCS)
AUX_SOURCE_DIRECTORY(${SRC_PATH}/core/memory CORE_MEMORY_SRCS)
AUX_SOURCE_DIRECTORY(${SRC_PATH}/core/module CORE_MODULE_SRCS)
AUX_SOURCE_DIRECTORY(${SRC_PATH}/core/monitor CORE_MONITOR_SRCS)
AUX_SOURCE_DIRECTORY(${SRC_PATH}/core/mutex CORE_MUTEX_SRCS)
AUX_SOURCE_DIRECTORY(${SRC_PATH}/core/network CORE_NETWORK_SRCS)
AUX_SOURCE_DIRECTORY(${SRC_PATH}/core/sem CORE_SEM_SRCS)
AUX_SOURCE_DIRECTORY(${SRC_PATH}/core/service_bus CORE_SERVICE_BUS_SRCS)
AUX_SOURCE_DIRECTORY(${SRC_PATH}/core/service_module CORE_SERVICE_MODULE_SRCS)
AUX_SOURCE_DIRECTORY(${SRC_PATH}/core/service_module/pc CORE_SERVICE_MODULE_PC_SRCS)
AUX_SOURCE_DIRECTORY(${SRC_PATH}/core/storage CORE_STORAGE_SRCS)
AUX_SOURCE_DIRECTORY(${SRC_PATH}/core/timer_service CORE_TIMER_SERVICE_SRCS)

AUX_SOURCE_DIRECTORY(${SRC_PATH}/service_core/codec SERVICE_CORE_CODEC_SRCS)
AUX_SOURCE_DIRECTORY(${SRC_PATH}/service_core/codec/thrift SERVICE_CORE_CODEC_THRIFT_SRCS)
AUX_SOURCE_DIRECTORY(${SRC_PATH}/service_core/common SERVICE_CORE_COMMON)
AUX_SOURCE_DIRECTORY(${SRC_PATH}/service_core/network SERVICE_CORE_NETWORK_SRCS)
AUX_SOURCE_DIRECTORY(${SRC_PATH}/service_core/p2pnet SERVICE_CORE_P2PNET_SRCS)

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

#cmake -D DEBUG_MODE=ON for release or debug
IF(DEBUG_MODE)
	ADD_DEFINITIONS("${CMAKE_CXX_FLAGS} ${MATRIX_FLAGS} -O0 -Wall -g -ggdb -DDEBUG")	
else()
	ADD_DEFINITIONS("${CMAKE_CXX_FLAGS} ${MATRIX_FLAGS} -O3 -Wall –DNDEBUG")
ENDIF()

############################################################################

#add target source
ADD_EXECUTABLE(${OUTPUT_NAME} 
${DIR_SRCS} 
${API_SRCS} 
${CORE_ALARM_SRCS} 
${CORE_CODEC_SRCS} 
${CORE_COMMON_SRCS} 
${CORE_CONF_SRCS} 
${CORE_CONF_VALIDATOR_SRCS}
${CORE_CONTAINER}
${CORE_CRYPTO}
${CORE_DATA_STRU_SRCS}
${CORE_ENV_SRCS} 
${CORE_INITIATOR_SRCS} 
${CORE_LOG_SRCS} 
${CORE_MATH_SRCS} 
${CORE_MEMORY_SRCS} 
${CORE_MODULE_SRCS} 
${CORE_MONITOR_SRCS} 
${CORE_MUTEX_SRCS} 
${CORE_NETWORK_SRCS} 
${CORE_SEM_SRCS} 
${CORE_SERVICE_BUS_SRCS} 
${CORE_SERVICE_MODULE_SRCS} 
${CORE_SERVICE_MODULE_PC_SRCS} 
${CORE_STORAGE_SRCS} 
${CORE_TIMER_SERVICE_SRCS} 
${SERVICE_CORE_CODEC_SRCS} 
${SERVICE_CORE_CODEC_THRIFT_SRCS}
${SERVICE_CORE_COMMON}
${SERVICE_CORE_NETWORK_SRCS}
${SERVICE_CORE_P2PNET_SRCS} 
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
#SET(LIBRARIES libm.so)
SET(LIBRARIES)

link_directories(${LIB_HOME_PATH}/linux/leveldb ${LIB_HOME_PATH}/linux/libevent ${LIB_HOME_PATH}/linux/openssl ${LIB_HOME_PATH}/linux/secp256k1)

#target link
MESSAGE(STATUS "${Boost_LIBRARIES}")
#libboost_thread.a -lboost_log_setup
TARGET_LINK_LIBRARIES(${OUTPUT_NAME} ${LIBRARIES} ${Boost_LIBRARIES} -levent -lleveldb -levent -levent_core -levent_openssl -levent_pthreads -lcrypto -lssl -lsecp256k1 libboost_thread.a)

############################################################################


MESSAGE(STATUS "end to cmake.........")

