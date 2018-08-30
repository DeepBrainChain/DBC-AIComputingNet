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
#make -j 4 dbc_core_test VERBOSE=1 2>&1 | tee  ../build_dbc_ut1.log
make -j 4 dbc_core_test 2>&1 | tee  ../build_dbc_ut.log
if [ ${PIPESTATUS[0]} -ne 0 ]
then
    echo "fail: see build_dbc_ut.log for more details"
    exit 1
fi

make -j 4 dbc_service_core_test 2>&1 | tee ../build_dbc_ut.log
if [ ${PIPESTATUS[0]} -ne 0 ]
then
    echo "fail: see build_dbc_ut.log for more details"
    exit 1
fi

make -j 4 dbc_service_test 2>&1 | tee ../build_dbc_ut.log
if [ ${PIPESTATUS[0]} -ne 0 ]
then
    echo "fail: see build_dbc_ut.log for more details"
    exit 1
fi

#
# run
#
echo "run dbc core test"
../../output/dbc_core_test  -r detailed ${@:1}

echo "run dbc service core test"
../../output/dbc_service_core_test  -r detailed -l all ${@:1}

echo "run dbc service test"
../../output/dbc_service_test  -r detailed -l all ${@:1}
