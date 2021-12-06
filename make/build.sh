#!/bin/bash

mkdir -p ./build
cd ./build

echo "cmake "
cmake -DCMAKE_BUILD_TYPE=Release ../.. > ../build_cmake.log 2>&1

if [ $? -ne 0 ]
then
    echo "fail: see build_cmake.log for more details"
    exit 1
fi

echo "make dbc"
SECONDS=0
make -j 4 dbc $1  2>&1 |tee ../build_make.log | grep -v "ld: warning: direct access"
if [ ${PIPESTATUS[0]} -ne 0 ]
then
    echo "fail: see build_make.log for more details"
    exit 1
fi

echo "take $SECONDS seconds to complete dbc building"

echo "make check_env"
make -j 4 check_env $1  2>&1 |tee ../build_make.log | grep -v "ld: warning: direct access"
if [ ${PIPESTATUS[0]} -ne 0 ]
then
    echo "fail: see build_make.log for more details"
    exit 1
fi
