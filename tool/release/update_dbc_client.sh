#!/bin/bash

release_version=0.3.7.4

cd /data

sudo rm -rf /data/dbc-linux-client*
# sudo wget https://github.com/DeepBrainChain/DBC-AIComputingNet/releases/download/0.3.7.3/dbc-linux-client-0.3.7.3.tar.gz
sudo wget https://github.com/DeepBrainChain/DBC-AIComputingNet/releases/download/$release_version/dbc-linux-client-$release_version.tar.gz
tar -zxvf /data/dbc-linux-client-$release_version.tar.gz


updateAbove0374()
{
  sudo rm -f $dbc_dir/dbc
  sudo rm -rf $dbc_dir/tool/
  sudo rm -rf $dbc_dir/conf/

  sudo cp ./dbc_client_node/dbc $dbc_dir
  sudo cp -rp ./dbc_client_node/tool $dbc_dir
  sudo cp -rp ./dbc_client_node/conf $dbc_dir

  echo "modify rest ip address"
  sudo sed -i "/http_ip=127.0.0.1/c http_ip=0.0.0.0" $dbc_dir/conf/core.conf
}

updateFrom0373()
{
  echo "current version 0.3.7.3"
  new_dir=$(dirname $dbc_dir)
  new_dir=$(dirname $new_dir)
  echo "new install path $new_dir"

  sudo mv ./dbc_client_node $new_dir/dbc_client_node

  new_dir=$new_dir/dbc_client_node

  echo "copy dbc client dat and leveldb"
  sudo cp -rp $dbc_dir/dat $new_dir
  sudo cp -rp $dbc_dir/db $new_dir

  echo "modify rest ip address"
  sudo sed -i "/http_ip=127.0.0.1/c http_ip=0.0.0.0" $new_dir/conf/core.conf

  echo "update dbc-client.service"
  sudo sed -i "/WorkingDirectory=/c WorkingDirectory=$new_dir" /lib/systemd/system/dbc-client.service
  sudo sed -i "/ExecStart=/c ExecStart=$new_dir/dbc --daemon" /lib/systemd/system/dbc-client.service
  sudo sed -i "/ExecStop=/c ExecStop=$new_dir/stopapp" /lib/systemd/system/dbc-client.service
  sudo systemctl daemon-reload

  echo "update DBC_CLIENT_PATH"
  sed -i "/DBC_CLIENT_PATH=/c export DBC_CLIENT_PATH=$new_dir" ~/.bashrc
  sed -i "s/export PATH=\$PATH:\$DBC_CLIENT_PATH.*//g" ~/.bashrc
  source ~/.bashrc

# sudo rm -f /home/dbc/0.3.7.3/dbc_repo/dbc
# sudo rm -rf /home/dbc/0.3.7.3/dbc_repo/tool/
# sudo rm -rf /home/dbc/0.3.7.3/dbc_repo/conf

# sudo cp /data/0.3.7.3/dbc_repo/dbc /home/dbc/0.3.7.3/dbc_repo/
# sudo cp -rp /data/0.3.7.3/dbc_repo/tool /home/dbc/0.3.7.3/dbc_repo/
# sudo cp -rp /data/0.3.7.3/dbc_repo/conf /home/dbc/0.3.7.3/dbc_repo/
}


dbc_dir=`cat ~/.bashrc | grep "DBC_CLIENT_PATH=" | awk -F '=' '{print $2}'`
if [ "$dbc_dir" == "" ]; then
  echo "no dbc client install found"
else
  echo "find dbc client path $dbc_dir"

  sudo systemctl stop dbc-client.service
  sudo systemctl stop dbc-client.service
  sudo systemctl stop dbc-client.service

  if [[ "$dbc_dir" == *0.3.7.3* ]]; then
    updateFrom0373
  else
    updateAbove0374
  fi

  sudo systemctl restart dbc-client.service
  echo "update complete"
fi

sudo rm -rf /data/dbc_client_node
sudo rm -rf /data/dbc-linux-client-$release_version.tar.gz
