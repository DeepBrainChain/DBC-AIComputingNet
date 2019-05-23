#!/bin/bash
test -d ./db_thrift_cpp && rm -rf ./db_thrift_cpp
test -d ./matrix_thrift_cpp && rm -rf ./matrix_thrift_cpp

mkdir ./db_thrift_cpp
mkdir ./matrix_thrift_cpp

thrift --gen cpp -out ./db_thrift_cpp ai_db.thrift
thrift --gen cpp -out ./matrix_thrift_cpp matrix.thrift
