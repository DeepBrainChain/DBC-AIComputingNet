#!/bin/bash

release_version=0.3.7.4
host=$(hostname)

download()
{
  echo "begin download"
  sudo rm -rf /data/dbc-linux-mining*
  # sudo wget https://github.com/DeepBrainChain/DBC-AIComputingNet/releases/download/0.3.7.3/dbc-linux-mining-0.3.7.3.tar.gz
  sudo wget https://github.com/DeepBrainChain/DBC-AIComputingNet/releases/download/$release_version/dbc-linux-mining-$release_version.tar.gz
  tar -zxvf /data/dbc-linux-mining-$release_version.tar.gz -C /data
}

deletePackage()
{
  sudo rm -rf /data/dbc_mining_node
  sudo rm -rf /data/dbc-linux-mining-$release_version.tar.gz
}

updateAbove0374()
{
  sudo rm -f $dbc_dir/dbc
  sudo rm -rf $dbc_dir/tool/
  sudo rm -rf $dbc_dir/conf/

  sudo cp /data/dbc_mining_node/dbc $dbc_dir
  sudo cp -rp /data/dbc_mining_node/tool $dbc_dir
  sudo cp -rp /data/dbc_mining_node/conf $dbc_dir
}

updateFrom0373()
{
  new_dir=$(dirname $dbc_dir)
  new_dir=$(dirname $new_dir)
  echo "new install path $new_dir"

  sudo mv /data/dbc_mining_node $new_dir/dbc_mining_node

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
  echo "find dbc path $dbc_dir"
  download

  sudo systemctl stop dbc
  sudo systemctl stop dbc
  sudo systemctl stop dbc

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

  sudo systemctl restart dbc

  deletePackage
  echo "update complete"
else
  echo "update failed"
  echo "Please copy update.sh to dbc's installed tool directory like /home/dbc/0.3.7.3/dbc_repo/tool/update.sh"
fi
