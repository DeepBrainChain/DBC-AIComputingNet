#!/bin/bash


gpu_info=""
all_gpu_info="N/A"
function get_all_gpus_info {

    echo "INFO: collect gpu info, start"

    if ! which nvidia-smi>/dev/null; then
        echo "ERROR: command nvidia-smi not found"
        return 1
    fi

    nvidia-smi -L > /tmp/dbc_gpuinfo
    input="/tmp/dbc_gpuinfo"


    gpu_list_str=""
    gpu_num=0

    while IFS= read -r line
    do
      get_gpu_info "$line"

      if [ -n "$gpu_info" ]; then

        if [ $gpu_num == 0 ]; then
            gpu_list_str="$gpu_info"
        else
            gpu_list_str="$gpu_list_str"", $gpu_info"
        fi

        let "gpu_num+=1"
      fi

    done < "$input"

    driver=$(nvidia-smi --query-gpu=driver_version --format=csv | uniq | tr '\n' ' ' | awk '{print $2}')

    cuda=$(nvidia-smi |grep -i cuda | awk '{print $(NF-1)}')

    # gpu p2p test
    gpu_p2p="nok"
    echo "INFO: GPU P2P test"
    if $DIR/simpleP2P &>/dev/null; then
        gpu_p2p="ok"
    fi

    all_gpu_info="{\"num\":\"$gpu_num\", \"driver\":\"$driver\", \"cuda\":\"$cuda\", \"p2p\":\"$gpu_p2p\", \"gpus\":[$gpu_list_str]}"

    echo "INFO: collect gpu info, end"

#    echo $all_gpu_info
}


gpu_bandwidth="N/A"
function get_gpu_bandwidth {

    echo "INFO: gpu $1 bandwidth test, start"

    v=$($DIR/bandwidthTest --device=$1 --htod --csv | grep bandwidth |awk -F "," '{print $2}' | awk -F "=" '{print $2}'|xargs)

    if [ "$v" != "" ]; then
        gpu_bandwidth=$v
    fi

    echo "INFO: gpu $1 bandwidth test, end"
}

function get_gpu_info {

    #GPU 0: GeForce 940MX (UUID: GPU-914b7cac-4d5f-60a9-7abb-aee06a91176c)
    data=$1

    uuid=$(echo "$data" |  awk 'match($0,/\(UUID: .+\)/) {print substr($0,RSTART+7,RLENGTH-8)}' )

    type=$(echo "$data" | awk -F "(" '{print $1}' | awk -F ":" '{print $2}'| tr -d ' ')

    id=$(echo "$data" | awk -F "(" '{print $1}' | awk -F ":" '{print $1}' | awk '{print $2}')

    if [ "$id" != "" ]; then

        get_gpu_bandwidth $id

        let "n=id+2"
        gpu_mem=$(nvidia-smi --query-gpu=memory.total --format=csv| head -$n | tail -1)

        gpu_info="{\"id\":\"$id\", \"type\":\"$type\", \"mem\":\"$gpu_mem\", \"bandwidth\":\"$gpu_bandwidth\"}"
    fi
}

speed_download="N/A"
speed_upload="N/A"
function speed_test {
    echo "INFO: network speed test, start"

    fn=/tmp/dbc_speedtest.log

    echo > $fn
    python $DIR/speedtest.py > $fn

    speed_download=$(grep "Download:" /tmp/dbc_speedtest.log | awk -F ": " '{print $2}')
    speed_upload=$(grep "Upload:" /tmp/dbc_speedtest.log | awk -F ": " '{print $2}')

    echo "INFO: network speed test, end"
}

public_ip="N/A"
function get_public_ip {
    echo "INFO: get public ip, start"

    myip=$(dig +short myip.opendns.com @resolver1.opendns.com)
    if [ -n "$myip" ]; then
        public_ip=$myip
    fi
    echo "INFO: get public ip: end"
}


disk_info="N/A"
function get_disk_info {
    echo "INFO: get disk info, start"
    dbc_container_path=$(docker info 2>&1| grep "Docker Root Dir" | awk -F ":" '{print $2}')

    if [ -z $dbc_container_path ]; then
        return 1
    fi

    raw_info=$(df -l -h $dbc_container_path | tail -1)

    disk_size=$(echo $raw_info | awk '{print $2}')

    free_disk_size=$(echo $raw_info | awk '{print $4}')

    device=$(echo $raw_info | awk '{print $1}' | awk -F"/" '{print $3}')

    disk_type=$(lsblk -o name,rota | grep $device | awk '{if($1=="1")print "HDD"; else print "SSD"}')


    v=$(dd if=/dev/zero of=/tmp/dbc_speed_test_output bs=8k count=10k 2>&1)
    disk_speed=$(echo $v | awk '{print $(NF-1)" " $NF}')

    rm -f /tmp/dbc_speed_test_output

    disk_info="{\"size\":\"$disk_size\", \"free\":\"$free_disk_size\", \"type\":\"$disk_type\", \"speed\":\"$disk_speed\"}"

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
        value=$(free -g -h | grep "Mem:" | awk '{print $2}')
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

# gpus info
get_all_gpus_info
write "gpu" "$all_gpu_info"

#  network speed
max_retry=3
for ((i = 0 ; i < $max_retry ; i++)); do
    speed_test

    if [ "$speed_download" != "" ] && [ "$speed_upload" != "" ]; then
        break
    fi

    let "c=max_retry-1"
    if [ $i -lt $c ]; then
        echo "ERROR: speed test failed, waiting 5 seconds and retry"
        sleep 5s
    fi
done

write "network_dl" "$speed_download"
write "network_ul" "$speed_upload"


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
