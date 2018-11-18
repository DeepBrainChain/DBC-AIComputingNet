#!/usr/bin/env bash

#criteria
function get_gpu_power {
    tp=$1

    case "$tp" in
    "GeForceGTX1080Ti")
        echo 113
    ;;

    "GeForce940MX")
        echo 10
    ;;

    "TeslaP100-PCIE-16GB")
        echo 100
    ;;

    "TeslaV100")
        echo 150
    ;;

    "TeslaP4")
        echo 55
    ;;

    "TeslaP40")
        echo 70
    ;;

    "GeForceGTX1080")
        echo 88
    ;;

    "GeForceGTX2080")
        echo 101
    ;;

    "GeForceGTX2080Ti")
        echo 134
    ;;

    *)
        echo 0
    ;;
    esac
}


#collect info
function get {
    attr=$1

    case "$attr" in
    "cpu_socket")
        cat /proc/cpuinfo 2>&1 | grep -i "physical id" | sort | uniq | wc -l
    ;;
    "cpu_core")
        cat /proc/cpuinfo 2>&1 |grep "model name" | awk -F ":" '{print $2}' | wc -l
    ;;

    "cpu_type")
        cat /proc/cpuinfo 2>&1 |grep "model name" | awk -F ":" '{print $2}' | uniq | xargs
    ;;

    "gpu_driver")
        if ! which nvidia-smi>/dev/null; then echo "N/A";
        else
            nvidia-smi --query-gpu=driver_version --format=csv | uniq  | grep -v "driver"
        fi
    ;;

    "gpu_type")
        if [ ! -d /proc/driver/nvidia/gpus ]; then echo "N/A";
        else
            v=$(for i in `ls /proc/driver/nvidia/gpus`; do cat /proc/driver/nvidia/gpus/$i/information | grep Model | awk '{out=""; for(i=2;i<=NF;i++){out=out$i}; print out}' ;done)
            echo "$v" | sort | uniq
        fi
    ;;

    "gpu_number")
        type=$2
        if [ ! -d /proc/driver/nvidia/gpus ]; then echo "0";
        else
            v=$(for i in `ls /proc/driver/nvidia/gpus`; do cat /proc/driver/nvidia/gpus/$i/information | grep Model | awk '{out=""; for(i=2;i<=NF;i++){out=out$i}; print out}' ;done)
            echo "$v" | grep $type | wc -l
        fi
    ;;

    "gpu_processing_power")
    # signle precision
        gpu_p_total=0
        gpu_t=$(get "gpu_type")
        for i in "$gpu_t"; do
            gpu_n=$(get "gpu_number" "$i")
            gpu_p=$(get_gpu_power $gpu_t)
            ((p=$gpu_p*$gpu_n))
            ((gpu_p_total+=$p))
        done

        echo $gpu_p_total
    ;;

    "gpu_usage")
        if ! which nvidia-smi>/dev/null; then echo "N/A";
        else
            nvidia-smi --query-gpu=utilization.gpu,utilization.memory --format=csv | grep -v "utilization" | awk -F "," '{print "gpu: "$1 "\nmem:"$2}'
        fi
    ;;

    "free_mem")
        if ! which free>/dev/null; then
            echo "free cmd not installed"
            return 1
        fi

        free -g | grep "Mem:" | awk '{print $NF}' | awk -F "G" '{print $1}'
    ;;

    "total_mem")
        free -g | grep "Mem:" | awk '{print $2}'
    ;;

    "disk")
        DIR=$(cd $( dirname "${BASH_SOURCE[0]}" ) >/dev/null && pwd)

        if [ -f $DIR/conf/container.conf ]; then
            dbc_container_path=$(grep "host_volum_dir" $DIR/conf/container.conf | grep -v "#")
        fi

        if [ -z $dbc_container_path ]; then
            dbc_container_path=$(docker info 2>&1| grep "Docker Root Dir" | awk -F ":" '{print $2}')
            if [ -z $dbc_container_path ]; then dbc_container_path=/; fi
        else
            dbc_container_path=$(echo $dbc_container_path | awk -F '=' '{print $2}')
            if [ -z $dbc_container_path ]; then
               dbc_container_path=$(docker info 2>&1| grep "Docker Root Dir" | awk -F ":" '{print $2}')
            fi
        fi

        if [ $(uname) = "Darwin" ]; then
            df -l -g $dbc_container_path | grep -v Filesystem | awk '{print $4}' | awk -F "G" '{print $1}'
        else

            df -l -BG $dbc_container_path | grep -v Filesystem | awk '{print $4}' | awk -F "G" '{print $1}'
        fi

    ;;

    "docker_version")
        if ! which docker>/dev/null; then
            echo "not installed"
            return 1
        fi

        if ! docker info >/dev/null 2>&1; then
            echo "not running"
            return 1
        fi

        docker info 2>&1 | grep "Server Version" | awk -F ":" '{print $2}' |xargs

    ;;

    "nvidia_docker_version")
        if ! which nvidia-docker>/dev/null; then
            echo "not installed"
            return 1
        fi

        if  ! nvidia-docker version &>/dev/null; then
            echo "not running"
            return 2
        fi

        nvidia-docker version | grep "NVIDIA Docker" | awk -F ":" '{print $2}' |xargs
    ;;

    *)
    echo "N/A"
    return 1
    ;;
    esac

    return 0
}


function cmp_version
{
    expect=$1
    actual=$2

    major_version_ref=$(echo $expect | awk -F "." '{print $1}')
    minor_version_ref=$(echo $expect | awk -F "." '{print $2}')

    major_version=$(echo $actual | awk -F "." '{print $1}')
    minor_version=$(echo $actual | awk -F "." '{print $2}')

    if [ $major_version -gt $major_version_ref ]; then
        return 0
    fi

    if [ $major_version -eq $major_version_ref ] && [ $minor_version -ge $minor_version_ref ]; then
        return 0
    fi

    return 1
}


function check_result
{
    code=$1
    if [ $code -eq 0 ]; then
        echo "pass"
        return 0
    else
        echo "failed with error code " $code
        return 1
    fi

}

echo "---------------------------------------"
echo "   step 1: check docker service        "
echo "---------------------------------------"

# docker version
docker_version_ref=18.03

v=$(get "docker_version")
if [ $? -eq 0 ] &&  cmp_version $docker_version_ref $v; then
    echo "docker        version  $v pass"
    netstat -na |grep ":31107 " | grep LISTEN >/dev/null
    if [ $? -ne 0 ]; then
        echo "Error: docker does not listen on port 31107"
    else
        echo "docker        service  pass"
    fi
else
    echo "docker        version  fail: " "expect $docker_version_ref, " "but $v"
fi


# nvidia docker version
nv_docker_version_ref=1.0
nv_docker_version=$(get "nvidia_docker_version")
nv_docker_major_version=0

if [ $? -eq 0 ] && cmp_version $nv_docker_version_ref $nv_docker_version; then
    echo "nvidia-docker version $nv_docker_version pass"
    nv_docker_major_version=$(echo $nv_docker_version | awk -F "." '{print $1}')
    if [ $nv_docker_major_version -eq 1 ]; then
        netstat -na |grep ":3476 " | grep LISTEN >/dev/null
        if [ $? -ne 0 ]; then
            echo "Error: nvidia docker does not listen on port 3476"
        else
            echo "nvidia-docker service  pass"
        fi
    fi
else
    echo "nvidia-docker version  fail: " "expect $nv_docker_version_ref, " "but $nv_docker_version"
fi


echo
echo "---------------------------------------"
echo "   step 2: check hardware              "
echo "---------------------------------------"

# cpu
cpu_core_ref=4
v=$(get "cpu_core")
((score=v*100/cpu_core_ref))

if [ $score -ge 100 ]; then
    echo "cpu_core score " $score " pass"
else
    echo "cpu_core score " $score " fail"
fi


# memory G
echo
mem_ref=10
v=$(get "free_mem")
if [ $? -eq 0 ]; then
    ((score=v*100/mem_ref))
else
    score=0
fi

if [ $score -ge 100 ]; then
    echo "memory   score " $score " pass"
else
    echo "memory   score " $score "  fail"
fi

# disk  G
echo
disk_ref=100
v=$(get "disk")
((score=v*100/disk_ref))


if [ $score -ge 100 ]; then
    echo "disk     score " $score " pass"
else
    echo "disk     score " $score "  fail"
fi


# gpu TFlops
echo
gpu_ref=70
v=$(get "gpu_processing_power")
((score=$v*100/gpu_ref))


if [ $score -ge 100 ]; then
    echo "gpu      score " $score " pass"
else
    echo "gpu      score " $score "  fail"
fi

# gpu
gpu_t=$(get "gpu_type")
for i in "$gpu_t"; do
    gpu_n=$(get "gpu_number" "$i")
    gpu_p=$(get_gpu_power $gpu_t)
    echo "gpu device: $gpu_n * $gpu_t"
done


echo
echo "---------------------------------------"
echo "   step 3: check dbc service           "
echo "---------------------------------------"

netstat -na |grep ":21107 " | grep LISTEN >/dev/null
if [ $? -ne 0 ]; then
    echo "Error: dbc does not listen on port 21107"
else
    echo "dbc        service  pass"
fi


echo
echo "---------------------------------------"
echo "   step 4: ipfs                        "
echo "---------------------------------------"

workspace=`pwd`
USERID=`id -u`
PROC_NAME="ipfs"
ps_="ps -e -o uid -o pid -o command"

pid=$($ps_  | grep [d]aemon | awk '{if (($1 == "'${USERID}'") && ($3~/'${PROC_NAME}'$/)) print $2}')
if [[ -n "$pid" ]]; then
    echo "failed, ipfs daemon shall not run outside of container!"
    exit 1
else
    echo "pass"
fi

echo
echo "---------------------------------------"
echo "   step 5: docker hub                  "
echo "---------------------------------------"

echo "test docker hello-world"
docker run --rm hello-world 2>&1 | grep Hello
check_result $?


# pull image
SECONDS=0
image_size=20

images=(python:2.7.15-alpine3.8)

for i in ${images[@]}; do
    echo
#    docker rmi $i >/dev/null
    echo "test docker hub downloading speed"
    docker pull $i

    check_result $?
done

 ((speed=$image_size*1024/$SECONDS))
 echo "download speed: $speed KB/s"


echo
echo "---------------------------------------"
echo "   step 6: nvidia docker container     "
echo "---------------------------------------"
# nvidia container
if [ $nv_docker_major_version -eq 1 ]; then
    echo "# nvidia-docker run --rm nvidia/cuda:8.0-runtime nvidia-smi"
    nvidia-docker run --rm nvidia/cuda:8.0-runtime nvidia-smi
fi

if [ $nv_docker_major_version -eq 2 ]; then
    echo "# docker run --runtime=nvidia --rm nvidia/cuda:8.0-runtime nvidia-smi"
    docker run --runtime=nvidia --rm nvidia/cuda:8.0-runtime nvidia-smi #&>/dev/null
fi

if ! check_result $?; then
    exit 1
fi

echo
echo "---------------------------------------"
echo "   step 7: AI processing power         "
echo "---------------------------------------"

dbc_path=$(grep DBC_PATH= ~/.bashrc | awk -F "=" '{print $2}')

if [ -z dbc_path ] ; then
    echo "Error: no DBC_PATH defined"
fi

if [ ! -d $dbc_path/container ]; then
    echo "Error: no container folder under DBC_PATH"
fi

if [ -n dbc_path ] && [ -d $dbc_path/container ] && [ $nv_docker_major_version -gt 0 ]; then

    images=(dbctraining/caffe2-gpu-cuda8-py2:v1.0.0
        dbctraining/tensorflow1.9.0-cuda9-gpu-py3:v1.0.0)

    for i in ${images[@]}; do
        echo
        echo "pull image " $i
        docker pull $i

        check_result $?
    done

    SECONDS=0

    mount_dir="$dbc_path/container:/home/dbc_utils:ro"

    # mnist
    image=dbctraining/caffe2-gpu-cuda8-py2:v1.0.0
    code_hash=QmauCdJXwR8eKb2aCF8fPqy9dyS8RUcAt1xisXKLzsLmUr
    data_hash=QmaDsKmXyEUV7WyutHHmoSU37Sg5ikbz3JcmmKx3DqkVA8
    entry=run.sh


    # hello world
    image=dbctraining/caffe2-gpu-cuda8-py2:v1.0.0
    code_hash=QmeS5G3iz92gvQCSsJ3K5wbZnFcXmEnBqyJGgbn4Wosqaj
    data_hash=QmZUC9CbaxRSdufUfFb178zAk25UJycrPnFkvY1sGF3tjY
    entry=caffe2_test.sh

    image=dbctraining/tensorflow1.9.0-cuda9-gpu-py3:v1.0.0
    code_hash=Qme2HkPuyhZ1MJGFU58qo6LBGAybAW4isCTEcofzd7rj5n
#    code_hash=QmTtTjgbA88MKDNktipTN4aVF79NRtUqgsAAAGTV3wFqZU
    data_hash=""
    entry=run.sh

    log_file=/tmp/benchmark.log
    rm -f $log_file
    if [ $nv_docker_major_version -eq 1 ]; then
        echo "# nvidia-docker run --rm nvidia/cuda:8.0-runtime nvidia-smi"
        nvidia-docker run --rm -v $mount_dir $image /home/dbc_utils/dbc_task.sh "$data_hash" "$code_hash" $entry \
        >$log_file 2>&1 &

        pid=$!
    fi

    if [ $nv_docker_major_version -eq 2 ]; then
#        docker run --runtime=nvidia --rm -v $mount_dir $image /home/dbc_utils/dbc_task.sh $data_hash $code_hash run.sh \
#        | tee /tmp/benchmark.log | grep "ipfs\|task\|download\|speed" | grep -v "removed"

        docker run --runtime=nvidia --rm -v $mount_dir $image /home/dbc_utils/dbc_task.sh "$data_hash" "$code_hash" $entry \
        >$log_file 2>&1 &

        pid=$!
    fi

    echo "start training, and save log into $log_file"
    echo "takes several minutes, and waiting for training result..."

    #wait %1

    while true; do
        get "gpu_usage" | xargs -l1 echo "    "

        if ! kill -0 $pid &>/dev/null; then
            echo "task end"
            break
        else
            sleep 5
        fi
    done

    echo "check training result"
    grep -i "error" $log_file |xargs -l1 echo "    "
    grep "end to exec dbc_task.sh and ready to say goodbye! :-)" $log_file > /dev/null
    check_result $?

    echo "It takes $SECONDS seconds to complete the ai training task."

else
    echo "Error: fail to run AI training"
fi


echo
