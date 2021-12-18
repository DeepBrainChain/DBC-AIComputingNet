#!/bin/bash

if [ $# -ne 1 ]; then
	echo "usage: $0 [install_dir]"
	exit 0
fi

download_url=http://121.57.95.175:20027/index.html/dbc
install_dir=$1
mkdir -p ${install_dir}
install_dir=$(cd ${install_dir}; pwd)

# check params
if [ "$(ls -A $install_dir)" ]; then
     echo "$install_dir is not empty"
     exit 0
fi

workpath=$(cd $(dirname $0) && pwd)
cd $workpath

# close old dbc_client_node
sudo systemctl stop restart-dbc-client.service
sudo systemctl disable restart-dbc-client.service

process_num=`ps -ef |grep -v grep |grep "restart_dbc_client.sh" | wc -l`
if [ ${process_num} -ne 0 ]
then
    pids=`ps -ef |grep -v grep |grep "restart_dbc_client.sh" |awk '{print $2}'`
    for pid in $pids
    do
        kill -9 $pid
    done
fi

sudo rm -f /etc/systemd/system/restart-dbc-client.service
sudo rm -f /etc/systemd/system/restart_dbc_client.sh

sudo systemctl stop dbc-client.service
sudo systemctl disable dbc-client.service
sudo rm -f /lib/systemd/system/dbc-client.service

# install deps
sudo apt-get install libvirt-clients libvirt-daemon-system -y

# install
mkdir -p install_cache
rm -rf install_cache/*

# download version file
wget -P ./install_cache ${download_url}/version
if [ $? -ne 0 ]; then
  echo "install failed: wget version file failed!"
  exit 1
fi
source ./install_cache/version

# download package file
node_file=dbc_client_node_${latest_version}.tar.gz
if [[ ! -f "./install_cache/${node_file}" ]]; then
  wget -P ./install_cache $download_url/${node_file}
  if [ $? -ne 0 ]; then
    echo "install failed: wget ${node_file} failed!"
    exit 1
  fi
fi
tar -zxvf ./install_cache/${node_file} -C ./install_cache/
cp -r ./install_cache/dbc_client_node/* $install_dir/

# install
old_tcp_listen_port=$(cat $install_dir/conf/core.conf | grep "net_listen_port=" | awk -F '=' '{print $2}')
old_http_listen_port=$(cat $install_dir/conf/core.conf | grep "http_port=" | awk -F '=' '{print $2}')

read -t 30 -e -p "输入tcp监听端口号 (端口范围：5000 - 5100, default:5001):" new_tcp_listen_port
if [ -z "$new_tcp_listen_port" ]; then
  new_tcp_listen_port="5001"
fi

read -t 30 -e -p "输入http监听端口号 (端口范围：5000 - 5100, default:5002):" new_http_listen_port
if [ -z "$new_http_listen_port" ]; then
  new_http_listen_port="5002"
fi

sed -i "/http_ip=127.0.0.1/c http_ip=0.0.0.0" $install_dir/conf/core.conf
sed -i "s/net_listen_port=${old_tcp_listen_port}/net_listen_port=${new_tcp_listen_port}/" $install_dir/conf/core.conf
sed -i "s/http_port=${old_http_listen_port}/http_port=${new_http_listen_port}/" $install_dir/conf/core.conf

chmod +x $install_dir/dbc
chmod +x $install_dir/shell/*.sh
chmod +x $install_dir/shell/crontab/*.sh

# start
/bin/bash $install_dir/shell/start.sh
sleep 3
process_client_num=`ps -ef | grep -v grep | grep  "$install_dir/shell/../dbc" | wc -l`
if [ ${process_client_num} -eq 0 ]
then
    echo "install failed: ${install_dir} start failed!"
    exit 1
fi

# set autorestart
echo "set autorestart..."
if [[ -d "/home/crontab" && -f "/home/crontab/crontab.off" ]]; then
  # close monitor
  crontab /home/crontab/crontab.off
  echo "wait local monitor exit"
  for((i=0; i<90; i++));
  do
    monitor_count=`ps aux | grep ";/bin/bash ./dbcmonitor.sh" | grep -v grep | wc -l`
    if [ ${monitor_count} -eq 0 ]
    then
        break
    fi

    echo -n "."
    sleep 1
  done

  monitor_count=`ps aux | grep ";/bin/bash ./dbcmonitor.sh" | grep -v grep | wc -l`
  if [ ${monitor_count} -ne 0 ]; then
    echo -e "\ninstall failed: local monitor exit failed!"
    exit 1
  else
    echo -e "\nlocal monitor exit success!"
  fi
fi

if [[ -d "/home/crontab" && -f "/home/crontab/dbc_process.conf" ]]; then
  rm -rf ./install_cache/dbc_client_node/dbc_dir_old.conf
  cp /home/crontab/dbc_process.conf ./install_cache/dbc_client_node/dbc_dir_old_cache.conf
  for  fline  in  `cat ./install_cache/dbc_client_node/dbc_dir_old_cache.conf`; do
    if [[ $fline == *=* ]]; then
      ffullpath=`echo $fline|awk -F'=' '{print $2}'|awk -F'"' '{print $2}'`
      echo $ffullpath >> ./install_cache/dbc_client_node/dbc_dir_old.conf
    else
      echo $fline >> ./install_cache/dbc_client_node/dbc_dir_old.conf
    fi
  done
  rm -rf ./install_cache/dbc_client_node/dbc_dir_old_cache.conf
fi

if [[ -d "/home/crontab" && -f "/home/crontab/dbc_dir.conf" ]]; then
  rm -rf ./install_cache/dbc_client_node/dbc_dir_old.conf
  cp /home/crontab/dbc_dir.conf ./install_cache/dbc_client_node/dbc_dir_old_cache.conf
  for  fline  in  `cat ./install_cache/dbc_client_node/dbc_dir_old_cache.conf`; do
    if [[ $fline == *=* ]]; then
      ffullpath=`echo $fline|awk -F'=' '{print $2}'|awk -F'"' '{print $2}'`
      echo $ffullpath >> ./install_cache/dbc_client_node/dbc_dir_old.conf
    else
      echo $fline >> ./install_cache/dbc_client_node/dbc_dir_old.conf
    fi
  done
  rm -rf ./install_cache/dbc_client_node/dbc_dir_old_cache.conf
fi

rm -rf /home/crontab
cp -rp ./install_cache/dbc_client_node/shell/crontab /home
if [[ -f "./install_cache/dbc_client_node/dbc_dir_old.conf" ]]; then
  cat ./install_cache/dbc_client_node/dbc_dir_old.conf > /home/crontab/dbc_dir.conf
fi

install_dir_fullpath=$(cd ${install_dir}; pwd)
if [ -z "`grep "$install_dir_fullpath" /home/crontab/dbc_dir.conf`" ]; then
  echo $install_dir_fullpath >> /home/crontab/dbc_dir.conf
fi

crontab /home/crontab/crontab.on
echo -e "local monitor open success!"

echo "install dbc client node success!"
