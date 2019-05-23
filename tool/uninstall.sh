#!/usr/bin/env bash

#echo "step 1: remove nvidia docker"
#nv_docker_major_ver=$(nvidia-docker version | grep "NVIDIA Docker" | awk '{print $3}' | awk -F "." '{print $1}')
#
#if [ "$nv_docker_major_ver" == "2" ]; then
#    echo "nvidia docker 2"
#fi
#
#sudo apt-get purge nvidia-docker
#sudo apt-get purge nvidia-docker2
#
#
#echo "step 2: remove docker"
#sudo apt-get purge docker


#!/bin/bash
#set -x

release_version=0.3.5.4
host=$(hostname)


#echo "begin to wget DBC release package"
#wget https://github.com/DeepBrainChain/deepbrainchain-release/releases/download/latest/dbc-linux-mining-$release_version.tar.gz
#if [ $? -ne 0 ]; then
#    echo "***wget DBC release package failed***"
#    exit
#fi
#tar -zxvf dbc-linux-mining-$release_version.tar.gz
#rm -rf dbc-linux-mining-$release_version.tar.gz
#echo "wget DBC release package finished"
#echo -e

cd ./$release_version
current_directory=`pwd`
echo "current directory is $current_directory "

echo "Execution script mining_install.sh to install docker,nvidia-docker,pull images(only for miner)"
docker volume ls -q -f driver=nvidia-docker | xargs -r -I{} -n1 docker ps -q -a -f volume={} | xargs -r docker rm -f
sudo apt-get -y purge nvidia-docker
sudo apt-get -y purge nvidia-docker2
sudo apt-get -y purge docker-ce
sudo rm /etc/docker

   cd ./mining_repo/
   #sed -i 's$echo y | sudo apt-get -y install docker-ce.*$echo y | sudo apt-get -y install docker-ce=18.06.1~ce~3-0~ubuntu$g' ./mining_install.sh
   /bin/bash ./mining_install.sh
   cd ./../
   echo "mining_install.sh execution finished"
   echo -e

echo "add nvidia-persistenced.service"
wget https://github.com/DeepBrainChain/deepbrainchain-release/releases/download/0.3.4.0/nvidia-persistenced.service
sudo cp ./nvidia-persistenced.service /lib/systemd/system/
sudo rm nvidia-persistenced.service
sudo systemctl daemon-reload
sudo systemctl enable nvidia-persistenced.service
sudo systemctl restart nvidia-persistenced.service


echo "begin to configure the DBC program container.conf item :host_volum_dir"
echo "below is your computer disk utilization:"
echo -e
df -hl

system_directory=`df -l | sort -n -r -k 4 |awk '{print $6}'`

a=0
for line in $system_directory
do
   array[$a]=$line
   a=$(($a+1))
done
echo -e

default_install_directory=`df -l | sort -n -r -k 4 |awk 'NR==1{print}'|awk '{print $6}'`

echo "Please choose your host_volum_dir directory,eg.you can input 0 if you want to set host_volum_dir as ${array[0]},recommend to choose the directory which has Maximal remaining space"
echo "***NOTE:if you ENTER directly the host_volum_dir default value is $default_install_directory (Maximal remaining space)"
echo "The directory below has already been descending order by remaining space "
echo -e

length=$((${#array[@]}-1))
loop=$(($length-1))

for(( i=0;i<$length;i++)) do
    echo [$i]:${array[$i]}
done
echo -e

read -p "Please input the number from 0 to $loop,can also input ENTER directly to use the default value:" num
while [[ $num -lt 0 || $num -gt $loop ]];do
   read -p "you have input a invalid number,it must be from 0 to $loop,please reinput:" num
done

if [ -z $num ];then
   echo "container.conf item :host_volum_dir will be set as default value:$default_install_directory"
   sudo rm -rf $default_install_directory/container_data_dir
   sudo mkdir $default_install_directory/container_data_dir
   if [ $? -ne 0 ]; then
      echo "mkdir error:maybe no authorization or readonly directory"
      exit
   fi
   sed -i "/host_volum_dir/c host_volum_dir=$default_install_directory/container_data_dir" ./dbc_repo/conf/container.conf
else
   echo "you have choosed number:$num,host_volum_dir will be set as ${array[$num]} "
   sudo rm -rf ${array[$num]}/container_data_dir
   sudo mkdir ${array[$num]}/container_data_dir
   if [ $? -ne 0 ]; then
      echo "mkdir error:maybe no authorization or readonly directory"
      exit
   fi
   sed -i "/host_volum_dir/c host_volum_dir=${array[$num]}/container_data_dir" ./dbc_repo/conf/container.conf

fi

echo "configure the container.conf item :host_volum_dir finished"
echo -e

echo "begin to  config container.conf shm_size"
memory_size=`cat /proc/meminfo | grep MemTotal | awk '{print $2}'`
rato=`expr 1024 \* 1024 \* 100 / 70`
shm_size=`expr $memory_size / $rato`
sed -i "/shm_size/c shm_size=$shm_size" ./dbc_repo/conf/container.conf

echo "show node id:"
cd ./dbc_repo/
./dbc --id
cd ./../
echo -e

echo "begin to add dbc executable path to ENV PATH"
cat ~/.bashrc | grep DBC_PATH
if [ $? -ne 0 ]; then
    echo "export DBC_PATH=$current_directory/dbc_repo" >> ~/.bashrc
    echo 'export PATH=$PATH:$DBC_PATH' >> ~/.bashrc
    echo 'export PATH=$PATH:$DBC_PATH/tool' >> ~/.bashrc
    echo "source .bashrc" >> ~/.bash_profile
else
    sed -i "/dbc_repo/c export DBC_PATH=$current_directory/dbc_repo" ~/.bashrc
fi
echo -e

echo "begin to set dbc auto-start when reboot/restart"
user_name=`whoami`
sudo rm -f /lib/systemd/system/dbc.service
sudo touch /lib/systemd/system/dbc.service
sudo chmod 777 /lib/systemd/system/dbc.service
echo "[Unit]" >> /lib/systemd/system/dbc.service
echo "Description=dbc Daemon" >> /lib/systemd/system/dbc.service
echo "Wants=network.target" >> /lib/systemd/system/dbc.service
echo "[Service]" >> /lib/systemd/system/dbc.service
echo "Type=forking" >> /lib/systemd/system/dbc.service
echo "User=$user_name" >> /lib/systemd/system/dbc.service
#echo "Group=$user_name" >> /lib/systemd/system/dbc.service
echo "WorkingDirectory=$current_directory/dbc_repo" >> /lib/systemd/system/dbc.service
echo "ExecStart=$current_directory/dbc_repo/dbc --ai --daemon -n \"$host\"" >> /lib/systemd/system/dbc.service
echo "ExecStop=$current_directory/dbc_repo/stopapp" >> /lib/systemd/system/dbc.service
echo "[Install]" >> /lib/systemd/system/dbc.service
echo "WantedBy=multi-user.target" >> /lib/systemd/system/dbc.service
sudo chmod 644 /lib/systemd/system/dbc.service

sudo systemctl daemon-reload
sudo systemctl enable dbc.service
#sudo systemctl restart dbc.service
echo -e

echo "disable system upgrade automatically"
sudo sed -i 's/1/0/g' /etc/apt/apt.conf.d/10periodic

echo "dbc ai mining install finished"
echo "run newgrp - docker and source ~/.bashrc to make DBC ENV effective"



