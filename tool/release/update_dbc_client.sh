#!/bin/bash

release_version=0.3.7.4

download()
{
  echo "begin download"
  sudo rm -rf /data/dbc-linux-client*
  # sudo wget https://github.com/DeepBrainChain/DBC-AIComputingNet/releases/download/0.3.7.3/dbc-linux-client-0.3.7.3.tar.gz
  sudo wget https://github.com/DeepBrainChain/DBC-AIComputingNet/releases/download/$release_version/dbc-linux-client-$release_version.tar.gz
  tar -zxvf /data/dbc-linux-client-$release_version.tar.gz -C /data
}

deletePackage()
{
  sudo rm -rf /data/dbc_client_node
  sudo rm -rf /data/dbc-linux-client-$release_version.tar.gz
}

updateAbove0374()
{
  sudo rm -f $dbc_dir/dbc
  sudo rm -rf $dbc_dir/tool/
  sudo rm -rf $dbc_dir/conf/

  sudo cp /data/dbc_client_node/dbc $dbc_dir
  sudo cp -rp /data/dbc_client_node/tool $dbc_dir
  sudo cp -rp /data/dbc_client_node/conf $dbc_dir

  echo "modify rest ip address"
  sudo sed -i "/http_ip=127.0.0.1/c http_ip=0.0.0.0" $dbc_dir/conf/core.conf
}

updateFrom0373()
{
  new_dir=$(dirname $dbc_dir)
  new_dir=$(dirname $new_dir)
  echo "new install path $new_dir"

  sudo mv /data/dbc_client_node $new_dir/dbc_client_node

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

  dbc_dir=$(dirname $dbc_dir)
  dbc_backup_dir="${dbc_dir}_backup_$(date +%F-%H%M%S)"
  echo "mv $dbc_dir $dbc_backup_dir"
  mv $dbc_dir $dbc_backup_dir

# sudo rm -f /home/dbc/0.3.7.3/dbc_repo/dbc
# sudo rm -rf /home/dbc/0.3.7.3/dbc_repo/tool/
# sudo rm -rf /home/dbc/0.3.7.3/dbc_repo/conf

# sudo cp /data/0.3.7.3/dbc_repo/dbc /home/dbc/0.3.7.3/dbc_repo/
# sudo cp -rp /data/0.3.7.3/dbc_repo/tool /home/dbc/0.3.7.3/dbc_repo/
# sudo cp -rp /data/0.3.7.3/dbc_repo/conf /home/dbc/0.3.7.3/dbc_repo/
}

cur_dir=$(cd "$(dirname "$0")";pwd)
dbc_dir=$(dirname $cur_dir)
# if [[ "$cur_dir" == *dbc*tool* ]]; then
if [ -f $dbc_dir/dbc ]; then
  echo "find dbc client path $dbc_dir"
  download

  sudo systemctl stop dbc-client.service
  sudo systemctl stop dbc-client.service
  sudo systemctl stop dbc-client.service

  cur_ver=$(cat $dbc_dir/conf/core.conf | grep "version=" | awk -F '=' '{print $2}')
  if [ "$cur_ver" == "" ]; then
    echo "no version in conf/core.conf"
    updateFrom0373
  else
    echo "current version $cur_ver and release version $release_version"
    cmp_ver=`expr $release_version \>= $cur_ver`
    if [ $cmp_ver -eq 1 ]; then
      # echo ">= version"
      updateAbove0374
    else
      # echo "< version"
      updateFrom0373
    fi
  fi

  sudo systemctl restart dbc-client.service

  deletePackage
  echo "update complete"
else
  echo "update failed"
  echo "Please copy update.sh to dbc's installed tool directory like /home/dbc/0.3.7.3/dbc_repo/tool/update.sh"
fi
