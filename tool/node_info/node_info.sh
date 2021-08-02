#!/bin/bash

public_ip="N/A"
function get_public_ip {
    echo "INFO: get public ip, start"
    myip=$(curl -s myip.ipip.net | awk -F ' ' '{print $2}' | awk -F '：' '{print $2}')
    i=1
    while [[ -z "$myip" && $i -le 10 ]]
    do
      myip=$(dig +short myip.opendns.com @resolver1.opendns.com)
      let i++
      sleep 1
    done

    if [ -n "$myip" ]; then
        public_ip=$myip
    fi
    echo "INFO: get public ip: end"
}

disk_info="N/A"
function get_disk_info {
    echo "INFO: get disk info, start"
    dbc_data_path='/data'

    if [ -z $dbc_data_path ]; then
        return 1
    fi

    raw_info=$(df -l -k $dbc_data_path | tail -1)

    disk_size=$(echo $raw_info | awk '{print $2}')
    n_disk_size=$(echo "scale=2; ${disk_size}/1024/1024/1024" | bc)

    free_disk_size=$(echo $raw_info | awk '{print $4}')
    n_disk_free_size=$(echo "scale=2; ${free_disk_size}/1024/1024/1024" | bc)

    device=$(echo $raw_info | awk '{print $1}' | awk -F"/" '{print $3}')

    disk_type=$(lsblk -o name,rota | grep $device | awk '{if($1=="1")print "HDD"; else print "SSD"}' | head -1)

    v=$(dd if=/dev/zero of=/tmp/dbc_speed_test_output bs=8k count=10k 2>&1)
    disk_speed=$(echo $v | awk '{print $(NF-1)}')
    disk_speed_unit=$(echo $v | awk '{print $NF}')

    if [ "${disk_speed_unit}" == "GB/s" ]; then
        x=$(python3 -c "print ($disk_speed*1000*1000)")
        disk_speed=$(printf "%.0f" $x)
    elif [ "${disk_speed_unit}" == "MB/s" ]; then
        x=$(python3 -c "print ($disk_speed*1000)")
        disk_speed=$(printf "%.0f" $x)
    fi

    rm -f /tmp/dbc_speed_test_output

    disk_info="{\"size\":\"${n_disk_size}T\", \"free\":\"${n_disk_free_size}T\", \"type\":\"$disk_type\", \"speed\":\"$disk_speed\"}"

    echo "INFO: get disk info, end"
}

cpu_info="N/A"
function get_cpu_info {
    echo "INFO: get cpu info, start"

    cpu_num=$(cat /proc/cpuinfo |grep "model name" | awk -F ":" '{print $2}'| wc -l)

    cpu_type=$(cat /proc/cpuinfo |grep "model name" | awk -F ":" '{print $2}'| uniq | awk -F "@" '{print $1}' |xargs)

    cpu_info="{\"num\":\"$cpu_num\", \"type\":\"$cpu_type\"}"

    echo "INFO: get cpu info, end"
}

value="N/A"
function get {
    attr=$1

    echo "INFO: get $1"

    case "$attr" in
    "mem")
        value="$(free  -g | grep "Mem:" | awk '{print $2}')G"
    ;;

    "os")
        version=$(uname -o -r)
        if [ -f /etc/os-release ]; then
            value=$(cat /etc/os-release | grep PRETTY_NAME | awk -F '=' '{print $2}'| tr -d '"')
            value="$value ""$version"

        fi
    ;;

    *)
    value="N/A"
    ;;
    esac
}

function write {
    fn=$conf_fn

    if  grep "^$1=" $fn &>/dev/null; then
        sed -i "s|$1=.*|$1=$2|g" $fn
    else
        echo "$1=$2" >> $fn
    fi
}

DIR=$(cd $( dirname "${BASH_SOURCE[0]}" ) >/dev/null && pwd)
conf_fn=$DIR/../../.dbc_node_info.conf

#  public ip
get_public_ip
write "ip" "$public_ip"

# mem
get mem
write "mem" "$value"

# disk
get_disk_info
write "disk" "$disk_info"

# os
get os
write "os" "$value"

# cpu
get_cpu_info
write "cpu" "$cpu_info"
