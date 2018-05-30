#!/bin/bash

if [ ! -d ./cmake-build-debug ]
then
    mkdir ./cmake-build-debug
fi

cd ./cmake-build-debug;

echo "cmake "
cmake DEBUG_MODE=ON  --build ../.. > ../build_cmake.log 2>&1

if [ $? -ne 0 ]
then
    echo "fail: see build_cmake.log for more details"
    exit 1
fi

#
# make
#

echo "make dbc ut "
SECONDS=0
make -j 4 dbc_core_test VERBOSE=1 2>&1 | tee  ../build_dbc_ut1.log 
if [ $? -ne 0 ]
then
    echo "fail: see build_dbc_ut.log for more details"
    exit 1
fi

SECONDS=0
make -j 4 dbc_service_core_test VERBOSE=1 2>&1 | tee ../build_dbc_ut2.log 
if [ $? -ne 0 ]
then
    echo "fail: see build_dbc_ut.log for more details"
    exit 1
fi

#
# run
#
echo "run dbc core test"
./src/core/unittest/dbc_core_test  -r detailed ${@:1}

echo "run dbc service core test"
./src/service_core/unittest/dbc_service_core_test  -r detailed -l all ${@:1}
