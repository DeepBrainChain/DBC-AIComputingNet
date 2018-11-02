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


echo "make dbc"
SECONDS=0
#make -j 4 dbc VERBOSE=1 2>&1 |tee ../build_dbc.log
make -j 4 dbc $1  2>&1 |tee ../build_dbc.log | grep -v "ld: warning: direct access"
if [ ${PIPESTATUS[0]} -ne 0 ]
then
    echo "fail: see build_dbc.log for more details"
    exit 1
fi
echo "take $SECONDS seconds to complete dbc building"

