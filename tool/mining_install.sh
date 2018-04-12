#!/bin/bash

cpu_tensorflow_path="dbctraining/tensorflow-cpu-0.1.0:v1"
gpu_tensorflow_path="dbctraining/tensorflow-gpu-0.1.0:v1"


ubuntu_version=`lsb_release -r --short`
if [ $ubuntu_version != "16.04" ]; then
   echo "dismatch ubuntu16.04 version"
   exit
fi
echo "***check ubuntu version success***"


function version_lt() { test "$(echo "$@" | tr " " "\n" | sort -rV | head -n 1)" != "$1"; }
minimum_version="3.10.0"
kernel_version=`uname -r`
if version_lt $kernel_version $minimum_version; then
   echo "$kernel_version is less than $minimum_version"
   exit
fi
echo "***check kernel version success***"


echo y | sudo apt-get install docker
echo y | sudo apt-get install docker.io
echo "***install docker success***"


user_name=`whoami`
sudo gpasswd -a $user_name docker
if [ $? -eq 0 ]; then
    echo "add $user_name to docker success"
else
    echo "add docker user failed"
    exit
fi
echo "***add user to docker group success***"


gpu_flag=`lspci |grep -i nvidia`
if [ $? -eq 0 ]; then
    echo "need to download GPU-related tensorflow"
    docker pull $gpu_tensorflow_path
    if [ $? -eq 1 ]; then
        echo "pull fail"
	exit 
    fi     
else
    echo "need to download CPU-related tensorflow"
    docker pull $cpu_tensorflow_path
    if [ $? -eq 1 ]; then
        echo "pull fail"
	exit
    fi 
fi
echo "***pull tensorflow image success***"














