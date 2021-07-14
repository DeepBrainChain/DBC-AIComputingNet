#ifndef DBC_NODE_INFO_QUERY_SH_H
#define DBC_NODE_INFO_QUERY_SH_H

#include <string>

const static std::string bash_interface_str = R"(
#!/bin/bash
function get {
    attr=$1

    case "$attr" in
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

    *)
    echo "N/A"
    ;;
    esac
}

DIR=$(cd $( dirname "${BASH_SOURCE[0]}" ) >/dev/null && pwd)
get $@

if [ -z "$1" ]; then
        echo "$0 <attr>, e.g. $0 cpu_usage or mem_usage"
fi
)";

#endif