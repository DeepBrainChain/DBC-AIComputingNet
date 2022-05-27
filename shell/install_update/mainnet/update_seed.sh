#!/bin/bash

if [ $# -lt 1 ]; then
	echo "usage: $0 [install_dir] [install_dir] ..."
	exit 0
fi

sudo apt install expect -y

download_url=http://116.169.53.132:9000/dbc/package
dir_num=$#
arr_dir=($*)
cur_path=$(cd .; pwd)

workpath=$(cd $(dirname $0) && pwd)
cd $workpath

# close_old_dbc_client_service
service_count=$(systemctl list-unit-files --type=service | grep "restart-dbc-client.service" | wc -l)
if [ $service_count -ne 0 ]; then
  service_count=$(systemctl status restart-dbc-client.service | grep "Active" | grep "running" | wc -l)
  if [ $service_count -ne 0 ]; then
    systemctl stop restart-dbc-client.service
  fi
  systemctl disable restart-dbc-client.service
fi

process_num=$(ps -ef | grep -v grep | grep "restart_dbc_client.sh" | wc -l)
if [ ${process_num} -ne 0 ]; then
  pids=$(ps -ef | grep -v grep | grep "restart_dbc_client.sh" | awk '{print $2}')
  for pid in $pids; do
    kill -9 $pid
  done
fi

if [ -f "/etc/systemd/system/restart-dbc-client.service" ]; then
  rm -f /etc/systemd/system/restart-dbc-client.service
fi

if [ -f "/etc/systemd/system/restart_dbc_client.sh" ]; then
  rm -f /etc/systemd/system/restart_dbc_client.sh
fi

service_count=$(systemctl list-unit-files --type=service | grep "dbc-client.service" | wc -l)
if [ $service_count -ne 0 ]; then
  service_count=$(systemctl status dbc-client.service | grep "Active" | grep "running" | wc -l)
  if [ $service_count -ne 0 ]; then
    systemctl stop dbc-client.service
  fi
  systemctl disable dbc-client.service
fi

if [ -f "/lib/systemd/system/dbc-client.service" ]; then
  rm -f /lib/systemd/system/dbc-client.service
fi

mkdir -p update_cache
rm -rf update_cache/*

echo "download package file..."
# download version file
wget -P ./update_cache $download_url/version
if [ $? -ne 0 ]; then
  echo "file:${download_url}/version download failed!"
  echo "install failed!"
  exit 0
fi
source ./update_cache/version

# download package file
node_file=dbc_seed_node_${latest_version}.tar.gz
if [ ! -f "./update_cache/${node_file}" ]; then
  wget -P ./update_cache $download_url/${node_file}
  if [ $? -ne 0 ]; then
    echo "file:$download_url/${node_file} download failed!"
    echo "install failed!"
    exit 0
  fi
fi
tar -zxvf ./update_cache/${node_file} -C ./update_cache/

# close monitor
if [[ -d "/home/crontab" && -f "/home/crontab/crontab.off" ]]; then
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
    echo -e "\nlocal monitor exit failed!"
    exit 1
  else
    echo -e "\nlocal monitor exit success!"
  fi
fi

if [[ -d "/home/crontab" && -f "/home/crontab/dbc_process.conf" ]]; then
  rm -rf ./update_cache/dbc_seed_node/dbc_dir_old.conf
  cp /home/crontab/dbc_process.conf ./update_cache/dbc_seed_node/dbc_dir_old_cache.conf
  for  fline  in  `cat ./update_cache/dbc_seed_node/dbc_dir_old_cache.conf`; do
    if [[ $fline == *=* ]]; then
      ffullpath=`echo $fline|awk -F'=' '{print $2}'|awk -F'"' '{print $2}'`
      echo $ffullpath >> ./update_cache/dbc_seed_node/dbc_dir_old.conf
    else
      echo $fline >> ./update_cache/dbc_seed_node/dbc_dir_old.conf
    fi
  done
  rm -rf ./update_cache/dbc_seed_node/dbc_dir_old_cache.conf
fi

if [[ -d "/home/crontab" && -f "/home/crontab/dbc_dir.conf" ]]; then
  rm -rf ./update_cache/dbc_seed_node/dbc_dir_old.conf
  cp /home/crontab/dbc_dir.conf ./update_cache/dbc_seed_node/dbc_dir_old_cache.conf
  for  fline  in  `cat ./update_cache/dbc_seed_node/dbc_dir_old_cache.conf`; do
    if [[ $fline == *=* ]]; then
      ffullpath=`echo $fline|awk -F'=' '{print $2}'|awk -F'"' '{print $2}'`
      echo $ffullpath >> ./update_cache/dbc_seed_node/dbc_dir_old.conf
    else
      echo $fline >> ./update_cache/dbc_seed_node/dbc_dir_old.conf
    fi
  done
  rm -rf ./update_cache/dbc_seed_node/dbc_dir_old_cache.conf
fi

if [ -d "/home/crontab" ]; then
  rm -rf /home/crontab
fi
cp -rf ./update_cache/dbc_seed_node/shell/crontab /home
if [[ -f "./update_cache/dbc_seed_node/dbc_dir_old.conf" ]]; then
  cat ./update_cache/dbc_seed_node/dbc_dir_old.conf > /home/crontab/dbc_dir.conf
fi

# update
for ((i=1;i<=$dir_num;i++)); do
  arr_install_dir=${arr_dir[$i-1]}
  cd $cur_path
  install_dir=$(cd $arr_install_dir; pwd)
  cd $workpath

  if [[ ! -d "$install_dir" ]]; then
    echo "$install_dir not exist!"
    continue
  fi

  rm -rf $install_dir/update.sh

  source $install_dir/conf/core.conf

  # update
  /bin/bash $install_dir/shell/stop.sh

  # net_ip
  old_net_listen_ip=$(cat $install_dir/conf/core.conf | grep "net_listen_ip=" | awk -F '=' '{print $2}')
  if [ -z "$old_net_listen_ip" ]; then
    old_net_listen_ip=$(cat $install_dir/conf/core.conf | grep "host_ip=" | awk -F '=' '{print $2}')
  fi
  # net_port
  old_net_listen_port=$(cat $install_dir/conf/core.conf | grep "net_listen_port=" | awk -F '=' '{print $2}')
  if [ -z "$old_net_listen_port" ]; then
    old_net_listen_port=$(cat $install_dir/conf/core.conf | grep "main_net_listen_port=" | awk -F '=' '{print $2}')
  fi
  # http_ip
  old_http_ip=$(cat $install_dir/conf/core.conf | grep "http_ip=" | awk -F '=' '{print $2}')
  if [ -z "$old_http_ip" ]; then
    old_http_ip=$(cat $install_dir/conf/core.conf | grep "rest_ip=" | awk -F '=' '{print $2}')
  fi
  # http_port
  old_http_port=$(cat $install_dir/conf/core.conf | grep "http_port=" | awk -F '=' '{print $2}')
  if [ -z "$old_http_port" ]; then
    old_http_port=$(cat $install_dir/conf/core.conf | grep "rest_port=" | awk -F '=' '{print $2}')
  fi

  cp -fp $install_dir/conf/peer.conf ./update_cache/dbc_seed_node/peer_old.conf

  new_net_listen_ip=$(cat ./update_cache/dbc_seed_node/conf/core.conf | grep "net_listen_ip=" | awk -F '=' '{print $2}')
  new_net_listen_port=$(cat ./update_cache/dbc_seed_node/conf/core.conf | grep "net_listen_port=" | awk -F '=' '{print $2}')
  new_http_ip=$(cat ./update_cache/dbc_seed_node/conf/core.conf | grep "http_ip=" | awk -F '=' '{print $2}')
  new_http_port=$(cat ./update_cache/dbc_seed_node/conf/core.conf | grep "http_port=" | awk -F '=' '{print $2}')

  cp -rfp ./update_cache/dbc_seed_node/dbc $install_dir/
  cp -rfp ./update_cache/dbc_seed_node/shell $install_dir/
  cp -rfp ./update_cache/dbc_seed_node/conf $install_dir/

  sed -i "/net_type=/c net_type=mainnet" $install_dir/conf/core.conf
  sed -i "/net_flag=/c net_flag=0xF1E1B0F9" $install_dir/conf/core.conf
  sed -i "s/net_listen_ip=${new_net_listen_ip}/net_listen_ip=${old_net_listen_ip}/" $install_dir/conf/core.conf
  sed -i "s/net_listen_port=${new_net_listen_port}/net_listen_port=${old_net_listen_port}/" $install_dir/conf/core.conf
  sed -i "s/http_ip=${new_http_ip}/http_ip=${old_http_ip}/" $install_dir/conf/core.conf
  sed -i "s/http_port=${new_http_port}/http_port=${old_http_port}/" $install_dir/conf/core.conf
  sed -i "/http_ip=0.0.0.0/c http_ip=127.0.0.1" $install_dir/conf/core.conf
  sed -i '/dbc_chain_domain=/d' $install_dir/conf/core.conf
  sed -i '/dbc_monitor_server=/d' $install_dir/conf/core.conf
  sed -i '/miner_monitor_server=/d' $install_dir/conf/core.conf
  sed -i '/image_server=/d' $install_dir/conf/core.conf

  cat ./update_cache/dbc_seed_node/peer_old.conf >> $install_dir/conf/peer.conf

  chmod +x $install_dir/dbc
  chmod +x $install_dir/shell/*.sh
  chmod +x $install_dir/shell/crontab/*.sh

  /bin/bash $install_dir/shell/start.sh

  install_dir_fullpath=$(cd ${install_dir}; pwd)
  if [ -z "`grep "$install_dir_fullpath" /home/crontab/dbc_dir.conf`" ]; then
    echo $install_dir_fullpath >> /home/crontab/dbc_dir.conf
  fi

  echo "update $install_dir success!"
done

crontab /home/crontab/crontab.on
echo -e "local monitor open success!"

echo "update all dbc seed nodes success!"
