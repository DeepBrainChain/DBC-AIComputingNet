#!/bin/bash

release_version=0.3.7.4
host=$(hostname)

cd /data

sudo rm -rf /data/dbc-linux-mining*
# sudo wget https://github.com/DeepBrainChain/DBC-AIComputingNet/releases/download/0.3.7.3/dbc-linux-mining-0.3.7.3.tar.gz
sudo wget https://github.com/DeepBrainChain/DBC-AIComputingNet/releases/download/$release_version/dbc-linux-mining-$release_version.tar.gz
tar -zxvf /data/dbc-linux-mining-$release_version.tar.gz


updateAbove0374()
{
  sudo rm -f $dbc_dir/dbc
  sudo rm -rf $dbc_dir/tool/
  sudo rm -rf $dbc_dir/conf/

  sudo cp ./dbc_mining_node/dbc $dbc_dir
  sudo cp -rp ./dbc_mining_node/tool $dbc_dir
  sudo cp -rp ./dbc_mining_node/conf $dbc_dir
}

updateFrom0373()
{
  echo "current version 0.3.7.3"
  new_dir=$(dirname $dbc_dir)
  new_dir=$(dirname $new_dir)
  echo "new install path $new_dir"

  sudo mv ./dbc_mining_node $new_dir/dbc_mining_node

  new_dir=$new_dir/dbc_mining_node

  echo "copy dbc dat and leveldb"
  sudo cp -rp $dbc_dir/dat $new_dir
  sudo cp -rp $dbc_dir/db $new_dir

  echo "update dbc.service"
  sudo sed -i "/WorkingDirectory=/c WorkingDirectory=$new_dir" /lib/systemd/system/dbc.service
  sudo sed -i "/ExecStart=/c ExecStart=$new_dir/dbc --ai --daemon -n \"$host\"" /lib/systemd/system/dbc.service
  sudo sed -i "/ExecStop=/c ExecStop=$new_dir/stopapp" /lib/systemd/system/dbc.service
  sudo systemctl daemon-reload

  echo "update DBC_PATH"
  sed -i "/DBC_PATH=/c export DBC_PATH=$new_dir" ~/.bashrc
  sed -i "s/export PATH=\$PATH:\$DBC_PATH.*//g" ~/.bashrc
  source ~/.bashrc

# sudo rm -f /home/dbc/0.3.7.3/dbc_repo/dbc
# sudo rm -rf /home/dbc/0.3.7.3/dbc_repo/tool/
# sudo rm -rf /home/dbc/0.3.7.3/dbc_repo/conf

# sudo cp /data/0.3.7.3/dbc_repo/dbc /home/dbc/0.3.7.3/dbc_repo/
# sudo cp -rp /data/0.3.7.3/dbc_repo/tool /home/dbc/0.3.7.3/dbc_repo/
# sudo cp -rp /data/0.3.7.3/dbc_repo/conf /home/dbc/0.3.7.3/dbc_repo/
}


dbc_dir=`cat ~/.bashrc | grep "DBC_PATH=" | awk -F '=' '{print $2}'`
if [ "$dbc_dir" == "" ]; then
  echo "no dbc install found"
else
  echo "find dbc path $dbc_dir"

  sudo systemctl stop dbc
  sudo systemctl stop dbc
  sudo systemctl stop dbc

  if [[ "$dbc_dir" == *0.3.7.3* ]]; then
    updateFrom0373
  else
    updateAbove0374
  fi

  sudo systemctl restart dbc
  echo "update complete"
fi

sudo rm -rf /data/dbc_mining_node
sudo rm -rf /data/dbc-linux-mining-$release_version.tar.gz
