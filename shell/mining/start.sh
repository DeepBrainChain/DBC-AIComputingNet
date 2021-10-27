#!/bin/bash

workpath=$(cd $(dirname $0) && pwd)
cd $workpath

. ../conf/core.conf

#sed -i "/http_ip=127.0.0.1/c http_ip=0.0.0.0" ${workpath}/../conf/core.conf

dbc_file=${workpath}/../dbc

process_num=`ps -ef | grep -v grep | grep "${dbc_file}\b" | wc -l`
if [ ${process_num} -ne 0 ]
then
    echo "${dbc_file} is already running!"
else
    host=$(hostname)
    ${dbc_file} -a -d -n $host
fi

process_num=`ps -ef | grep -v grep | grep  "${dbc_file}\b" | wc -l`
if [ ${process_num} -eq 0 ]
then
    echo "${dbc_file} start failed!"
    exit 1
fi

echo "${dbc_file} start ok!"
