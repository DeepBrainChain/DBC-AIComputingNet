#!/bin/bash
#set -x

release_version=0.3.4.0

echo "begin to wget DBC release package"
wget https://github.com/DeepBrainChain/deepbrainchain-release/releases/download/0.3.4.0/dbc-linux-mining-0.3.4.0.tar.gz
if [ $? -ne 0 ]; then
    echo "***wget DBC release package failed***"
    exit
fi
tar -zxvf dbc-linux-mining-0.3.4.0.tar.gz
rm -rf dbc-linux-mining-0.3.4.0.tar.gz
echo "wget DBC release package finished"
echo -e

cd ./$release_version
current_directory=`pwd`
echo "current directory is $current_directory "

echo "Execution script mining_install.sh to install docker,nvidia-docker,pull images(only for miner)"
cd ./mining_repo/
/bin/bash ./mining_install.sh
cd ./../
echo "mining_install.sh execution finished"
echo -e


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
   sed -i "5c host_volum_dir=$default_install_directory/container_data_dir" ./dbc_repo/conf/container.conf
else
   echo "you have choosed number:$num,host_volum_dir will be set as ${array[$num]} "
   sudo rm -rf ${array[$num]}/container_data_dir
   sudo mkdir ${array[$num]}/container_data_dir
   if [ $? -ne 0 ]; then
      echo "mkdir error:maybe no authorization or readonly directory"
      exit
   fi
   sed -i "5c host_volum_dir=${array[$num]}/container_data_dir" ./dbc_repo/conf/container.conf
fi

echo "configure the container.conf item :host_volum_dir finished"
echo -e

echo "add dbc executable path to ENV PATH"
echo "export DBC_PATH=$current_directory/dbc_repo" >> ~/.bashrc
echo 'export PATH=$PATH:$DBC_PATH' >> ~/.bashrc
echo "export DBC_TOOL_PATH=$current_directory/dbc_repo/tool" >> ~/.bashrc 
echo 'export PATH=$PATH:$DBC_TOOL_PATH' >> ~/.bashrc
echo "source .bashrc" >> ~/.bash_profile

echo "begin to set dbc startapp auto-start when reboot/restart"
sudo chmod 777 /etc/rc.local
sudo echo "cd $current_directory/dbc_repo/" >> /etc/rc.local
sudo echo 'su dbc -c "./dbc --ai --daemon"' >> /etc/rc.local
echo -e

echo "begin to start DBC ai-server program"
cd ./dbc_repo/
./dbc --ai --daemon
cd ./../
echo "start DBC ai-server program finished"

echo "dbc ai user linux install finished,DBC AI server has already run as daemon mode"
echo "run source ~/.bashrc to make DBC ENV effective"
