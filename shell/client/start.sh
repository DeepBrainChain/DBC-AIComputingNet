#!/bin/bash

workpath=$(cd $(dirname $0) && pwd)
cd $workpath

. ../conf/core.conf

dbc_file=${workpath}/../dbc

process_num=`ps -ef | grep -v grep | grep "${dbc_file}\b" | wc -l`
if [ ${process_num} -ne 0 ]
then
    echo "${dbc_file} is already running!"
    exit 1
else
    ${dbc_file} -d
fi

process_num=`ps -ef | grep -v grep | grep  "${dbc_file}\b" | wc -l`
if [ ${process_num} -eq 0 ]
then
    echo "${dbc_file} start failed!"
    exit 1
fi

echo "${dbc_file} start ok!"
