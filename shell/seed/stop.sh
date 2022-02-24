#!/bin/bash

workpath=$(cd $(dirname $0) && pwd)
cd $workpath

. ../conf/core.conf

dbc_file=${workpath}/../dbc

process_num=`ps -ef |grep -v grep |grep "${dbc_file}\b" | wc -l`
if [ ${process_num} -ne 0 ]
then
    pids=`ps -ef |grep -v grep |grep "${dbc_file}\b" |awk '{print $2}'`
    for pid in $pids
    do
        kill -9 $pid
    done
fi

echo "${dbc_file} stop ok!"
