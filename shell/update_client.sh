#!/bin/bash

if [ $# -lt 1 ]; then
	echo "usage: $0 [install_dir] [install_dir] ..."
	exit 0
fi

dir_num=$#
arr_dir=($*)
cur_path=$(cd .; pwd)

workpath=$(cd $(dirname $0) && pwd)
cd $workpath

download_url=http://121.57.95.175:20027/index.html/dbc

# update
mkdir -p update_cache
rm -rf update_cache/*

# download version file
if [[ ! -f "./update_cache/version" ]]; then
  wget -P ./update_cache $download_url/version
  if [ $? -ne 0 ]; then
    echo "update failed: wget dbc version file failed!"
    exit 1
  fi
fi
source ./update_cache/version

# download package file
node_file=dbc_client_node_${latest_version}.tar.gz
if [[ ! -f "./update_cache/${node_file}" ]]; then
  wget -P ./update_cache $download_url/${node_file}
  if [ $? -ne 0 ]; then
    echo "update failed: wget ${node_file} failed!"
    exit 1
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
  rm -rf ./update_cache/dbc_client_node/dbc_dir_old.conf
  cp /home/crontab/dbc_process.conf ./update_cache/dbc_client_node/dbc_dir_old_cache.conf
  for  fline  in  `cat ./update_cache/dbc_client_node/dbc_dir_old_cache.conf`; do
    if [[ $fline == *=* ]]; then
      ffullpath=`echo $fline|awk -F'='` '{print $2}'|awk -F'"' '{print $2}'
      echo $ffullpath >> ./update_cache/dbc_client_node/dbc_dir_old.conf
    else
      echo $fline >> ./update_cache/dbc_client_node/dbc_dir_old.conf
    fi
  done
  rm -rf ./update_cache/dbc_client_node/dbc_dir_old_cache.conf
fi

if [[ -d "/home/crontab" && -f "/home/crontab/dbc_dir.conf" ]]; then
  rm -rf ./update_cache/dbc_client_node/dbc_dir_old.conf
  cp /home/crontab/dbc_dir.conf ./update_cache/dbc_client_node/dbc_dir_old_cache.conf
  for  fline  in  `cat ./update_cache/dbc_client_node/dbc_dir_old_cache.conf`; do
    if [[ $fline == *=* ]]; then
      ffullpath=`echo $fline|awk -F'='` '{print $2}'|awk -F'"' '{print $2}'
      echo $ffullpath >> ./update_cache/dbc_client_node/dbc_dir_old.conf
    else
      echo $fline >> ./update_cache/dbc_client_node/dbc_dir_old.conf
    fi
  done
  rm -rf ./update_cache/dbc_client_node/dbc_dir_old_cache.conf
fi

rm -rf /home/crontab
cp -rp ./update_cache/dbc_client_node/shell/crontab /home
if [[ -f "./update_cache/dbc_client_node/dbc_dir_old.conf" ]]; then
  cat ./update_cache/dbc_client_node/dbc_dir_old.conf > /home/crontab/dbc_dir.conf
fi

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
  if [[ -n ${version} && ("${latest_version}" < "${version}" || "${latest_version}" == "${version}") ]]
  then
      echo "$install_dir is already latest version!"
      continue
  fi

  # update
  /bin/bash $install_dir/shell/stop.sh

  old_net_listen_port=$(cat $install_dir/conf/core.conf | grep "net_listen_port=" | awk -F '=' '{print $2}')
  old_http_port=$(cat $install_dir/conf/core.conf | grep "http_port=" | awk -F '=' '{print $2}')

  cp -fp $install_dir/conf/peer.conf ./update_cache/dbc_client_node/peer_old.conf

  new_net_listen_port=$(cat ./update_cache/dbc_client_node/conf/core.conf | grep "net_listen_port=" | awk -F '=' '{print $2}')
  new_http_port=$(cat ./update_cache/dbc_client_node/conf/core.conf | grep "http_port=" | awk -F '=' '{print $2}')

  cp -rfp ./update_cache/dbc_client_node/dbc $install_dir/
  cp -rfp ./update_cache/dbc_client_node/shell $install_dir/
  cp -rfp ./update_cache/dbc_client_node/conf $install_dir/

  sed -i "/http_ip=127.0.0.1/c http_ip=0.0.0.0" $install_dir/conf/core.conf
  sed -i "s/net_listen_port=${new_net_listen_port}/net_listen_port=${old_net_listen_port}/" $install_dir/conf/core.conf
  sed -i "s/http_port=${new_http_port}/http_port=${old_http_port}/" $install_dir/conf/core.conf

  cat ./update_cache/dbc_client_node/peer_old.conf >> $install_dir/conf/peer.conf

  chmod +x $install_dir/dbc
  chmod +x $install_dir/shell/*.sh
  chmod +x $install_dir/shell/crontab/*.sh

  /bin/bash $install_dir/shell/start.sh
  sleep 3
  process_client_num=`ps -ef | grep -v grep | grep  "$install_dir/shell/../dbc" | wc -l`
  if [ ${process_client_num} -eq 0 ]
  then
      echo "update ${install_dir} failed: start failed!"
      continue
  fi

  install_dir_fullpath=$(cd ${install_dir}; pwd)
  if [ -z "`grep "$install_dir_fullpath" /home/crontab/dbc_dir.conf`" ]; then
    echo $install_dir_fullpath >> /home/crontab/dbc_dir.conf
  fi
done

crontab /home/crontab/crontab.on
echo -e "local monitor open success!"

echo "update all dbc client nodes success!"
