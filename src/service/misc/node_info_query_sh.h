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

const std::string bash_interface_str = R"(
#!/bin/bash
function get {
    attr=$1

    case "$attr" in
    "gpu_usage")
        if ! which nvidia-smi>/dev/null; then echo "N/A";
        else
            nvidia-smi --query-gpu=utilization.gpu,memory.used,memory.total --format=csv,noheader | awk -F "," '{print "gpu: "$1 "\nmem: "$2/$3*100" %"}'
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

    "mem_usage")
        free -m | grep "Mem:" | awk '{usage=($2-$NF)*100/$2} END {print usage "%"}'
    ;;


    "image")
        if ! which docker>/dev/null; then echo "N/A";
        else
            docker image list | grep dbctraining | grep -v none | awk '{print $1":"$2}'
        fi
    ;;

    "nvidia-smi")
        nvidia-smi
    ;;

    "docker")
        docker $2
    ;;

    *)
    echo "N/A"
    ;;
    esac
}

DIR=$(cd $( dirname "${BASH_SOURCE[0]}" ) >/dev/null && pwd)
get $@

if [ -z "$1" ]; then
        echo "$0 <attr>, e.g. $0 gpu"
fi
)";
