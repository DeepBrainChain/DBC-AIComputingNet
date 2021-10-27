#!/bin/bash

workpath=$(cd $(dirname $0) && pwd)
cd $workpath

. ./conf/core.conf

mkdir -p update_temp
rm -rf update_temp/*

# version
wget -P ./update_temp http://111.44.254.179:22244/dbc/version
if [ $? -ne 0 ]; then
  echo "wget dbc version failed!"
  exit 1
fi

source ./update_temp/version

if [[ -n ${version} && ("${latest_version}" < "${version}" || "${latest_version}" == "${version}") ]]
then
    echo "dbc is already latest!"
    exit 1
fi

# update
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

/bin/bash ./shell/stop.sh

os_name=`uname -a | awk '{print $1}'`
if [ $os_name == 'Linux' ];then
    os_name=linux
elif [ $os_name == 'Darwin' ];then
    os_name=macos
fi

client_file=dbc_${os_name}_client_${latest_version}.tar.gz

wget -P ./update_temp http://111.44.254.179:22244/dbc/${client_file}
if [ $? -ne 0 ]; then
  echo "wget dbc package failed!"
  exit 1
fi

tar -zxvf ./update_temp/${client_file} -C ./update_temp/

old_net_listen_port=$(cat $workpath/conf/core.conf | grep "net_listen_port=" | awk -F '=' '{print $2}')
old_http_port=$(cat $workpath/conf/core.conf | grep "http_port=" | awk -F '=' '{print $2}')

new_net_listen_port=$(cat ./update_temp/dbc_client_node/conf/core.conf | grep "net_listen_port=" | awk -F '=' '{print $2}')
new_http_port=$(cat ./update_temp/dbc_client_node/conf/core.conf | grep "http_port=" | awk -F '=' '{print $2}')

cp -rfp $workpath/conf/peer.conf ./update_temp/dbc_client_node/conf

cp -rfp ./update_temp/dbc_client_node/dbc $workpath/
cp -rfp ./update_temp/dbc_client_node/shell $workpath/
cp -rfp ./update_temp/dbc_client_node/conf $workpath/

sed -i "s/net_listen_port=${new_net_listen_port}/net_listen_port=${old_net_listen_port}/" $workpath/conf/core.conf
sed -i "s/http_port=${new_http_port}/http_port=${old_http_port}/" $workpath/conf/core.conf

chmod +x $workpath/dbc
chmod +x $workpath/shell/*.sh
chmod +x $workpath/shell/crontab/*.sh

crontab /home/crontab/crontab.on
/bin/bash ./shell/start.sh

echo "update dbc_client_node success!"
