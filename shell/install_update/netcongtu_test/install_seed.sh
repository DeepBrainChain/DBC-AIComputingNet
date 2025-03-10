#!/bin/bash

if [ $# -ne 1 ]; then
  echo "usage: $0 [install_dir]"
  exit 0
fi

download_url=http://112.192.16.27:9000/dbc/package
install_dir=$1
mkdir -p ${install_dir}
install_dir=$(
  cd ${install_dir}
  pwd
)

# check params
if [ "$(ls -A $install_dir)" ]; then
  echo "$install_dir is not empty"
  exit 0
fi

function close_ufw()
{
  is_ufw_enable=$(systemctl is-enabled ufw.service)
  if [ "$is_ufw_enable"x = "enabled"x ]; then
    sudo systemctl disable ufw.service --now
    sudo systemctl restart libvirtd.service
    echo "disable ufw service"
  fi

  ufw_status=$(ufw status | awk '{print $2}')
  if [ "$ufw_status"x = "active"x ]; then
    sudo ufw disable
    sudo systemctl restart libvirtd.service
    echo "disable ufw"
  fi
}

close_ufw

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

# install_deps
echo "install deps..."
sudo apt-get install libvirt-clients libvirt-daemon-system expect -y

mkdir -p install_cache
rm -rf install_cache/*

echo "download package file..."
# download version file
wget -P ./install_cache ${download_url}/version
if [ $? -ne 0 ]; then
  echo "file:${download_url}/version download failed!"
  echo "install failed!"
  exit 0
fi
source ./install_cache/version

# download package file
node_file=dbc_seed_node_${latest_version}.tar.gz
if [ ! -f "./install_cache/${node_file}" ]; then
  wget -P ./install_cache $download_url/${node_file}
  if [ $? -ne 0 ]; then
    echo "file:$download_url/${node_file} download failed!"
    echo "install failed!"
    exit 0
  fi
fi
tar -zxvf ./install_cache/${node_file} -C ./install_cache/
cp -r ./install_cache/dbc_seed_node/*  $install_dir/

# install_dbc_seed
echo "installing..."
old_tcp_listen_port=$(cat $install_dir/conf/core.conf | grep "net_listen_port=" | awk -F '=' '{print $2}')
old_http_listen_port=$(cat $install_dir/conf/core.conf | grep "http_port=" | awk -F '=' '{print $2}')

echo "输入tcp监听端口号 (端口范围：5000 - 5100, default:5001):"
read -t 60 -e new_tcp_listen_port
if [ -z "$new_tcp_listen_port" ]; then
  new_tcp_listen_port="5001"
fi

echo "输入http监听端口号 (端口范围：5000 - 5100, default:5002):"
read -t 60 -e new_http_listen_port
if [ -z "$new_http_listen_port" ]; then
  new_http_listen_port="5002"
fi

sed -i "/net_type=/c net_type=netcongtu_test" $install_dir/conf/core.conf
sed -i "/net_flag=/c net_flag=0xF1E1B1E8" $install_dir/conf/core.conf
sed -i "s/net_listen_port=${old_tcp_listen_port}/net_listen_port=${new_tcp_listen_port}/" $install_dir/conf/core.conf
sed -i "s/http_port=${old_http_listen_port}/http_port=${new_http_listen_port}/" $install_dir/conf/core.conf
sed -i "/http_ip=0.0.0.0/c http_ip=127.0.0.1" $install_dir/conf/core.conf
sed -i '/image_server=/d' $install_dir/conf/core.conf
sed -i "/dbc_chain_domain=/d" $install_dir/conf/core.conf

chmod +x $install_dir/dbc
chmod +x $install_dir/shell/*.sh
chmod +x $install_dir/shell/crontab/*.sh

# start_dbc_seed
echo "start..."
/bin/bash $install_dir/shell/start.sh

# set_auto_restart
echo "set auto restart..."
# close monitor
if [[ -d "/home/crontab" && -f "/home/crontab/crontab.off" ]]; then
  crontab /home/crontab/crontab.off
  echo "wait local monitor exit"
  for ((i = 0; i < 90; i++)); do
    monitor_count=$(ps aux | grep ";/bin/bash ./dbcmonitor.sh" | grep -v grep | wc -l)
    if [ ${monitor_count} -eq 0 ]; then
      break
    fi

    echo -n "."
    sleep 1
  done

  monitor_count=$(ps aux | grep ";/bin/bash ./dbcmonitor.sh" | grep -v grep | wc -l)
  if [ ${monitor_count} -ne 0 ]; then
    echo -e "\nlocal monitor exit failed!"
    echo "install failed!"
    exit 0
  else
    echo -e "\nlocal monitor exit success!"
  fi
fi

if [[ -d "/home/crontab" && -f "/home/crontab/dbc_process.conf" ]]; then
  rm -rf ./install_cache/dbc_seed_node/dbc_dir_old.conf
  cp /home/crontab/dbc_process.conf ./install_cache/dbc_seed_node/dbc_dir_old_cache.conf
  for fline in $(cat ./install_cache/dbc_seed_node/dbc_dir_old_cache.conf); do
    if [[ $fline == *=* ]]; then
      ffullpath=$(echo $fline | awk -F'=' '{print $2}' | awk -F'"' '{print $2}')
      echo $ffullpath >>./install_cache/dbc_seed_node/dbc_dir_old.conf
    else
      echo $fline >>./install_cache/dbc_seed_node/dbc_dir_old.conf
    fi
  done
  rm -rf ./install_cache/dbc_seed_node/dbc_dir_old_cache.conf
fi

if [[ -d "/home/crontab" && -f "/home/crontab/dbc_dir.conf" ]]; then
  rm -rf ./install_cache/dbc_seed_node/dbc_dir_old.conf
  cp /home/crontab/dbc_dir.conf ./install_cache/dbc_seed_node/dbc_dir_old_cache.conf
  for fline in $(cat ./install_cache/dbc_seed_node/dbc_dir_old_cache.conf); do
    if [[ $fline == *=* ]]; then
      ffullpath=$(echo $fline | awk -F'=' '{print $2}' | awk -F'"' '{print $2}')
      echo $ffullpath >>./install_cache/dbc_seed_node/dbc_dir_old.conf
    else
      echo $fline >>./install_cache/dbc_seed_node/dbc_dir_old.conf
    fi
  done
  rm -rf ./install_cache/dbc_seed_node/dbc_dir_old_cache.conf
fi

if [ -d "/home/crontab" ]; then
  rm -rf /home/crontab
fi
cp -rf ./install_cache/dbc_seed_node/shell/crontab /home
if [[ -f "./install_cache/dbc_seed_node/dbc_dir_old.conf" ]]; then
  cat ./install_cache/dbc_seed_node/dbc_dir_old.conf > /home/crontab/dbc_dir.conf
fi

install_dir_fullpath=$(
  cd ${install_dir}
  pwd
)
if [ -z "$(grep "$install_dir_fullpath" /home/crontab/dbc_dir.conf)" ]; then
  echo $install_dir_fullpath >> /home/crontab/dbc_dir.conf
fi

crontab /home/crontab/crontab.on
echo -e "local monitor open success!"

echo "install dbc seed node success!"
