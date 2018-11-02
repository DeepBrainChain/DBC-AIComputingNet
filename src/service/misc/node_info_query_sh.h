/*********************************************************************************

*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name         : query_sh.h
* description       : 
* date              : 2018/6/15
* author            : Jimmy Kuang
**********************************************************************************/
#pragma once

#include <string>

const std::string node_info_query_sh_str = R"(
#!/bin/bash
function get {
    attr=$1

    case "$attr" in
    "gpu")
        if ! which nvidia-smi>/dev/null; then echo "N/A";
        else
            #nvidia-smi -L | awk -F "(" '{print $1}' | awk -F ":" '{print $2}' | uniq -c | awk '{out=$1" * "; for(i=2;i<=NF;i++){out=out$i}; print out}' | tr '\n' ' '
            v=$(for i in `ls /proc/driver/nvidia/gpus`; do cat /proc/driver/nvidia/gpus/$i/information | grep Model | awk '{out=""; for(i=2;i<=NF;i++){out=out$i}; print out}' ;done)
            echo $v | tr " " "\n" | uniq -c | awk '{print $1" * "$2}' | tr '\n' ' '
        fi
    ;;

    "cpu")
        cat /proc/cpuinfo |grep "model name" | awk -F ":" '{print $2}' | uniq -c
    ;;

    "gpu_usage")
        if ! which nvidia-smi>/dev/null; then echo "N/A";
        else
            nvidia-smi --query-gpu=utilization.gpu,utilization.memory --format=csv | grep -v "utilization" | awk -F "," '{print "gpu: "$1 "\nmem:"$2}'
        fi
    ;;

    "gpu_driver")
        if ! which nvidia-smi>/dev/null; then echo "N/A";
        else
            nvidia-smi --query-gpu=driver_version --format=csv | uniq | tr '\n' ' '
        fi
    ;;

    "cpu_usage")
        #grep 'cpu ' /proc/stat | awk '{usage=($2+$4)*100/($2+$4+$5)} END {print usage "%"}'
        read cpu a b c previdle rest < /proc/stat
        prevtotal=$((a+b+c+previdle))
        sleep 0.1
        read cpu a b c idle rest < /proc/stat
        total=$((a+b+c+idle))
        CPU=$((100*( (total-prevtotal) - (idle-previdle) ) / (total-prevtotal) ))
        echo "$CPU%"
    ;;

    "mem")
        free -g -h | grep "Mem:" | awk '{print "total: "$2, "free: "$NF}'
    ;;

    "mem_usage")
        free -m | grep "Mem:" | awk '{usage=($2-$NF)*100/$2} END {print usage "%"}'
    ;;

    "disk")
        DIR=$(cd $( dirname "${BASH_SOURCE[0]}" ) >/dev/null && pwd)
        dbc_container_path=$(grep "host_volum_dir" $DIR/conf/container.conf | grep -v "#")

        if [ -z $dbc_container_path ]; then
            dbc_container_path=$(docker info 2>1| grep "Docker Root Dir" | awk -F ":" '{print $2}')
        else
            dbc_container_path=$(echo $dbc_container_path | awk -F '=' '{print $2}')
            if [ -z $dbc_container_path ]; then
               dbc_container_path=$(docker info 2>1| grep "Docker Root Dir" | awk -F ":" '{print $2}')
            fi
        fi

        df -l -h $dbc_container_path
    ;;

    "image")
        if ! which docker>/dev/null; then echo "N/A";
        else
            docker image list | grep dbctraining | grep -v none | awk '{print $1":"$2}'
        fi
    ;;
    *)
    echo "N/A"
    ;;
    esac
}

for var in "$@"
do
#echo "[$var]"
get $var
done

if [ -z "$1" ]; then
        echo "$0 <attr>, e.g. $0 gpu"
fi
)";