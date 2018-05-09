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

echo y | sudo apt-get -y install apt-transport-https ca-certificates curl software-properties-common
if [ $? -eq 1 ]; then
    echo "***install Dependent Libraries error***"
    exit
fi
echo "***install third library success***"

curl -fsSL https://download.docker.com/linux/ubuntu/gpg | sudo apt-key add -
if [ $? -eq 1 ]; then
    echo "***curl apt-key error***"
    exit
fi
echo "***apt-key add success***"

sudo add-apt-repository \
       "deb [arch=amd64] https://download.docker.com/linux/ubuntu \
       $(lsb_release -cs) \
       stable"
sudo apt-get update
echo y | sudo apt-get -y install docker-ce
if [ $? -eq 1 ]; then
    echo "install docker-ce failed"
    exit
fi
echo "***install docker-ce success ***"

user_name=`whoami`
sudo gpasswd -a $user_name docker
if [ $? -eq 0 ]; then
    echo "add $user_name to docker success"
else
    echo "add docker user failed"
    exit
fi
echo "***add user to docker group success***"

sudo echo 'DOCKER_OPTS="-H unix:///var/run/docker.sock -H tcp://127.0.0.1:31107"' >> /etc/default/docker

gpu_flag=`lspci |grep -i nvidia`
if [ $? -eq 0 ]; then
    wget -P /tmp https://github.com/NVIDIA/nvidia-docker/releases/download/v1.0.1/nvidia-docker_1.0.1-1_amd64.deb
    if [ $? -eq 1 ]; then
        echo "***wget nvidia-docker deb failed***"
        exit
    fi
    echo "***wget nvidia-docker deb success ***"
    sudo dpkg -i /tmp/nvidia-docker*.deb && rm /tmp/nvidia-docker*.deb
    if [ $? -eq 1 ]; then
        echo "***dpkg nvidia-docker deb failed***"
        exit
    fi
    echo "***install nvidia-docker success ***"
fi

echo "***start to install nvidia cuda8.0 and cudnn6.0***"
gpu_flag=`lspci |grep -i nvidia`
if [ $? -eq 0 ]; then
    wget https://developer.nvidia.com/compute/cuda/8.0/Prod2/local_installers/cuda-repo-ubuntu1604-8-0-local-ga2_8.0.61-1_amd64-deb
    if [ $? -eq 1 ]; then
        echo "***wget nvidia-docker deb failed***"
        exit
    fi
    sudo dpkg -i cuda-repo-ubuntu1604-8-0-local-ga2_8.0.61-1_amd64.deb 
    sudo apt-get update 
    echo y | sudo apt-get install cuda 
    sudo echo "export PATH=/usr/local/cuda-8.0/bin:$PATH" >> /etc/profile
    sudo echo "export LD_LIBRARY_PATH=/usr/local/cuda-8.0/lib64$LD_LIBRARY_PATH" >> /etc/profile
    sudo source /etc/profile
    echo "***install nvidia cuda8.0 success ***"

    echo "***start to install nvidia cudnn6.0***"
fi

gpu_flag=`lspci |grep -i nvidia`
if [ $? -eq 0 ]; then
    echo "need to download GPU-related tensorflow"
    sudo docker pull $gpu_tensorflow_path
    if [ $? -eq 1 ]; then
        echo "pull fail"
	exit 
    fi 
    echo "***pull tensorflow GPU image success***"    
else
    echo "need to download CPU-related tensorflow"
    sudo docker pull $cpu_tensorflow_path
    if [ $? -eq 1 ]; then
        echo "pull fail"
	exit
    fi 
    echo "***pull tensorflow CPU image success***"
fi
