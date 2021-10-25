#!/bin/bash
#set -x

host=$(hostname)
dbc_repo_dir=dbc_mining_node

download_dbc_tar()
{
  echo "begin to wget DBC release package"
  rm -rf dbc-linux-mining*.tar.gz

  #wget https://github.com/DeepBrainChain/deepbrainchain-release/releases/download/latest/dbc-linux-mining-$release_version.tar.gz
  #wget http://116.85.24.172:20444/static/dbc-linux-mining-$release_version.tar.gz
  #wget https://github.com/DeepBrainChain/DBC-AIComputingNet/releases/download/$release_version/dbc-linux-mining-$release_version.tar.gz
  wget http://111.44.254.179:22244/dbc/dbc-linux-mining.tar.gz
  if [ $? -ne 0 ]; then
    echo "***wget DBC release package failed***"
    exit
  fi
  #tar -zxvf dbc-linux-mining-$release_version.tar.gz
  #rm -rf dbc-linux-mining-$release_version.tar.gz
  echo "wget DBC release package finished"
  echo -e
}

install_dbc()
{
  path=$1
  path="${path}/${dbc_repo_dir}"
  #cd ./$dbc_repo_dir
  path_now=`pwd`
  cd $path
  current_directory=`pwd`
  echo "current directory is $current_directory "

  cd ./mining_repo/
  /bin/bash ./mining_install.sh
  cd ./../
  echo "mining_install.sh execution finished"
  echo -e

  cd $path_now
}

uninstall_dbc()
{
  echo
  echo "-- move dbc install --"
  # dbc_dir=$(dirname  `which dbc` 2>/dev/null)
  dbc_dir=`cat ~/.bashrc | grep "DBC_PATH=" | awk -F '=' '{print $2}'`
  if [ "$dbc_dir" == "" ]; then
    echo "no dbc install found"
  else
    echo "find dbc path $dbc_dir"
    now_dir="$(pwd)"
    cd $dbc_dir
    dbc_dir=$(pwd)
    cd $now_dir

    echo
    echo "-- stop automatically"
    sudo systemctl stop restart_dbc.service
    sudo systemctl disable restart_dbc.service
    echo "-- remove automatically"
    sudo rm -f /etc/systemd/system/restart_dbc.service
    sudo rm -f /etc/systemd/system/restart_dbc.sh
    sudo systemctl daemon-reload
    sudo systemctl restart cron
    sudo systemctl enable cron

    echo "-- stop dbc auto-start"
    sudo systemctl stop dbc.service
    sudo systemctl disable dbc.service
    echo "-- remove dbc auto-start"
    sudo rm -f /lib/systemd/system/dbc.service

    sudo systemctl daemon-reload
    sudo systemctl reset-failed dbc.service

    dbc_backup_dir="${dbc_dir}_backup_$(date +%F-%H%M%S)"
    echo "mv $dbc_dir $dbc_backup_dir"
    mv $dbc_dir $dbc_backup_dir
  fi

  echo
  echo "-- remove dbc install path from bashrc --"
  sed -i "s/export DBC_PATH=.*//g" ~/.bashrc
  sed -i "s/export PATH=\$PATH:\$DBC_PATH.*//g" ~/.bashrc

  echo
  echo "complete"
}


post_config()
{
  echo "add nvidia-persistenced.service"
  #wget https://github.com/DeepBrainChain/deepbrainchain-release/releases/download/0.3.4.0/nvidia-persistenced.service

  path=$1
  path="${path}/${dbc_repo_dir}"
  path_now=`pwd`
  cd $path
  current_directory=`pwd`
  echo "current directory is $current_directory "

  sudo cp ./mining_repo/nvidia-persistenced.service /lib/systemd/system/
  #sudo rm nvidia-persistenced.service
  sudo systemctl daemon-reload
  sudo systemctl enable nvidia-persistenced.service
  sudo systemctl restart nvidia-persistenced.service

  echo "begin to  config container.conf shm_size"
  memory_size=`cat /proc/meminfo | grep MemTotal | awk '{print $2}'`
  rato=`expr 1024 \* 1024 \* 100 / 70`
  shm_size=`expr $memory_size / $rato`
  sed -i "/shm_size/c shm_size=$shm_size" ./conf/container.conf

  echo "show node id:"
  ./dbc --id
    echo "-----------------------------------------------------------"
    echo "Please input your dbc wallet address below"
    echo "CAUTION:the dbc address will be used in payment of rent."
    echo "please be sure that the wallet you filled is correct!"
    echo "-----------------------------------------------------------"
    read -p "please input your dbc wallet address:" wallet_addr
    if [ -z $wallet_addr ]
    then
      echo "wallet address is empty or invalid, please check and try again!"
      echo "please add your wallet address manually by editing ./conf/core.conf"
      echo "eg. wallet=AbpBWkCp3jcLZpvf5zCtCLRaGNRi95osik"
    fi
    echo "wallet=${wallet_addr}" >> ./conf/core.conf


  echo -e

  echo "begin to add dbc executable path to ENV PATH"
  cat ~/.bashrc | grep "DBC_PATH="
  if [ $? -ne 0 ]; then
    echo "export DBC_PATH=$current_directory" >> ~/.bashrc
    # echo 'export PATH=$PATH:$DBC_PATH' >> ~/.bashrc
    # echo 'export PATH=$PATH:$DBC_PATH/tool' >> ~/.bashrc
    echo "source .bashrc" >> ~/.bash_profile
  else
    sed -i "/DBC_PATH=/c export DBC_PATH=$current_directory" ~/.bashrc
  fi
  echo -e

  echo "begin to set dbc auto-start when reboot/restart"
  user_name=`whoami`
  sudo rm -f /lib/systemd/system/dbc.service
  sudo touch /lib/systemd/system/dbc.service
  sudo chmod 777 /lib/systemd/system/dbc.service
  echo "[Unit]" >> /lib/systemd/system/dbc.service
  echo "Description=dbc Daemon" >> /lib/systemd/system/dbc.service
  echo "Wants=network.target" >> /lib/systemd/system/dbc.service
  echo "[Service]" >> /lib/systemd/system/dbc.service
  echo "Type=forking" >> /lib/systemd/system/dbc.service
  echo "User=$user_name" >> /lib/systemd/system/dbc.service
  #echo "Group=$user_name" >> /lib/systemd/system/dbc.service
  echo "WorkingDirectory=$current_directory" >> /lib/systemd/system/dbc.service
  echo "ExecStart=$current_directory/dbc --ai --daemon -n \"$host\"" >> /lib/systemd/system/dbc.service
  echo "ExecStop=$current_directory/stopapp" >> /lib/systemd/system/dbc.service
  echo "[Install]" >> /lib/systemd/system/dbc.service
  echo "WantedBy=multi-user.target" >> /lib/systemd/system/dbc.service
  sudo chmod 644 /lib/systemd/system/dbc.service

  sudo systemctl daemon-reload
  sudo systemctl enable dbc.service
  #sudo systemctl restart dbc.service
  echo -e

  echo "disable system upgrade automatically"
  sudo sed -i 's/1/0/g' /etc/apt/apt.conf.d/10periodic

  # register private repo
  sudo bash ./container/lxcfs/install.sh

  echo "set restart dbc automatically"
  sudo chmod 777 ./tool/restart_dbc.sh
  sudo cp  ./tool/restart_dbc.sh /etc/systemd/system
  sudo chmod 777 /etc/systemd/system/restart_dbc.sh
  sudo cp  ./tool/restart_dbc.service /etc/systemd/system
  sudo chmod 777 /etc/systemd/system/restart_dbc.service
  sudo systemctl daemon-reload
  sudo systemctl start restart_dbc.service
  sudo systemctl enable restart_dbc.service
  sudo chmod 777 ./tool/clean_cache.sh
  sudo systemctl restart cron
  sudo systemctl enable cron
  echo "dbc ai mining install finished"
  source ~/.bashrc

  cd $path_now
}

usage() { echo "Usage: $0 [-d ] [-i path] [-u]" 1>&2; exit 1; }

while getopts "udi:" o; do
    case "${o}" in
        u)
			while true; do
			read -p "Do you wish to uninstall dbc(y/n)?" yn
			case $yn in
				[Yy]* ) uninstall_dbc; break;;
				[Nn]* ) exit;;
			* ) echo "Please answer yes or no.";;
			esac
			done
			exit 0
			;;
        d)
			echo "download install tar ball"
            download_dbc_tar
			exit 0
            ;;
        i)
            install_path=${OPTARG}
            echo "install"
			      dbc_tar=dbc-linux-mining.tar.gz
            if [ ! -f $dbc_tar ];then
              echo "$dbc_tar not found"
              exit 1
		      	fi
            if [ -d $install_path/$dbc_repo_dir ]; then
              echo "target install folder $install_path/$dbc_repo_dir is not empty!"
              exit 1
            fi
				    tar -zxvf $dbc_tar -C $install_path
			          install_dbc $install_path
                post_config $install_path
			exit 0
			;;
        *)
            usage
            ;;
    esac
done
shift $((OPTIND-1))

if [ -z "${s}" ] || [ -z "${p}" ]; then
    usage
fi

exit 0
