#!/bin/bash
#set -x

release_version=0.3.7.4
host=$(hostname)
dbc_repo_dir=dbc_client_node

download_dbc_tar()
{
  echo "begin to wget DBC release package"
  rm -rf dbc-linux-client-$release_version.tar.gz

  #wget https://github.com/DeepBrainChain/deepbrainchain-release/releases/download/latest/dbc-linux-client-$release_version.tar.gz
  #wget http://111.44.254.164:22244/dbc-linux-client-$release_version.tar.gz
  wget https://github.com/DeepBrainChain/DBC-AIComputingNet/releases/download/$release_version/dbc-linux-client-$release_version.tar.gz
  if [ $? -ne 0 ]; then
    echo "***wget DBC release package failed***"
    exit
  fi
  #tar -zxvf dbc-linux-client-$release_version.tar.gz
  #rm -rf dbc-linux-client-$release_version.tar.gz
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

}

uninstall_dbc()
{
  echo
  echo "-- move dbc install --"
  # dbc_dir=$(dirname  `which dbc` 2>/dev/null)
  dbc_dir=`cat ~/.bashrc | grep "DBC_CLIENT_PATH=" | awk -F '=' '{print $2}'`
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
    sudo systemctl stop restart-dbc-client.service
    sudo systemctl disable restart-dbc-client.service
    echo "-- remove automatically"
    sudo rm -f /etc/systemd/system/restart-dbc-client.service
    sudo rm -f /etc/systemd/system/restart_dbc_client.sh
    sudo systemctl daemon-reload
    sudo systemctl restart cron
    sudo systemctl enable cron

    echo "-- stop dbc auto-start"
    sudo systemctl stop dbc-client.service
    sudo systemctl disable dbc-client.service
    echo "-- remove dbc auto-start"
    sudo rm -f /lib/systemd/system/dbc-client.service

    sudo systemctl daemon-reload
    sudo systemctl reset-failed dbc-client.service

	  dbc_backup_dir="${dbc_dir}_backup_$(date +%F-%H%M%S)"
	  echo "mv $dbc_dir $dbc_backup_dir"
	  mv $dbc_dir $dbc_backup_dir
  fi

  echo
  echo "-- remove dbc install path from bashrc --"
  sed -i "s/export DBC_CLIENT_PATH=.*//g" ~/.bashrc
  sed -i "s/export PATH=\$PATH:\$DBC_CLIENT_PATH.*//g" ~/.bashrc

  echo
  echo "complete"
}


post_config()
{
  path=$1
  path="${path}/${dbc_repo_dir}"
  path_now=`pwd`
  cd $path
  current_directory=`pwd`
  echo "current directory is $current_directory "

  echo "begin to add dbc executable path to ENV PATH"
  cat ~/.bashrc | grep "DBC_CLIENT_PATH="
  if [ $? -ne 0 ]; then
    echo "export DBC_CLIENT_PATH=$current_directory" >> ~/.bashrc
    # echo 'export PATH=$PATH:$DBC_CLIENT_PATH' >> ~/.bashrc
    # echo 'export PATH=$PATH:$DBC_CLIENT_PATH/tool' >> ~/.bashrc
    echo "source .bashrc" >> ~/.bash_profile
  else
    sed -i "/DBC_CLIENT_PATH=/c export DBC_CLIENT_PATH=$current_directory" ~/.bashrc
  fi

  echo "modify rest ip address"
  sed -i "/http_ip=127.0.0.1/c http_ip=0.0.0.0" ./conf/core.conf

  echo -e

  echo "begin to set dbc auto-start when reboot/restart"
  user_name=`whoami`
  sudo rm -f /lib/systemd/system/dbc-client.service
  sudo touch /lib/systemd/system/dbc-client.service
  sudo chmod 777 /lib/systemd/system/dbc-client.service
  echo "[Unit]" >> /lib/systemd/system/dbc-client.service
  echo "Description=dbc client Daemon" >> /lib/systemd/system/dbc-client.service
  echo "Wants=network.target" >> /lib/systemd/system/dbc-client.service
  echo "[Service]" >> /lib/systemd/system/dbc-client.service
  echo "Type=forking" >> /lib/systemd/system/dbc-client.service
  echo "User=$user_name" >> /lib/systemd/system/dbc-client.service
  #echo "Group=$user_name" >> /lib/systemd/system/dbc-client.service
  echo "WorkingDirectory=$current_directory" >> /lib/systemd/system/dbc-client.service
  echo "ExecStart=$current_directory/dbc --daemon" >> /lib/systemd/system/dbc-client.service
  echo "ExecStop=$current_directory/stopapp" >> /lib/systemd/system/dbc-client.service
  echo "[Install]" >> /lib/systemd/system/dbc-client.service
  echo "WantedBy=multi-user.target" >> /lib/systemd/system/dbc-client.service
  sudo chmod 644 /lib/systemd/system/dbc-client.service

  sudo systemctl daemon-reload
  sudo systemctl enable dbc-client.service
  #sudo systemctl restart dbc-client.service
  echo -e

  echo "disable system upgrade automatically"
  sudo sed -i 's/1/0/g' /etc/apt/apt.conf.d/10periodic

  # register private repo
  # sudo bash ./container/lxcfs/install.sh

  echo "set restart dbc automatically"
  sudo chmod 777 ./tool/restart_dbc_client.sh
  sudo cp  ./tool/restart_dbc_client.sh /etc/systemd/system
  sudo chmod 777 /etc/systemd/system/restart_dbc_client.sh

  sudo rm -f /etc/systemd/system/restart-dbc-client.service
  sudo touch /etc/systemd/system/restart-dbc-client.service
  sudo chmod 777 /etc/systemd/system/restart-dbc-client.service
  echo "[Unit]" >> /etc/systemd/system/restart-dbc-client.service
  echo "Description=restart_dbc_client" >> /etc/systemd/system/restart-dbc-client.service
  echo "[Service]" >> /etc/systemd/system/restart-dbc-client.service
  echo "ExecStart=/bin/bash  /etc/systemd/system/restart_dbc_client.sh    &" >> /etc/systemd/system/restart-dbc-client.service
  echo "[Install]" >> /etc/systemd/system/restart-dbc-client.service
  echo "WantedBy=multi-user.target" >> /etc/systemd/system/restart-dbc-client.service
  sudo chmod 644 /etc/systemd/system/restart-dbc-client.service
  sudo systemctl daemon-reload
  sudo systemctl start restart-dbc-client.service
  sudo systemctl enable restart-dbc-client.service
  sudo chmod 777 ./tool/clean_cache.sh
  sudo systemctl restart cron
  sudo systemctl enable cron
  echo "dbc ai client install finished"
  source ~/.bashrc
  
  cd $path_now
}

usage() { echo "Usage: $0 [-d ] [-i path] [-u]" 1>&2; exit 1; }

while getopts "udi:" o; do
    case "${o}" in
        u)
			while true; do
			read -p "Do you wish to uninstall dbc client(y/n)?" yn
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
            echo "begin install dbc client"
			      dbc_tar=dbc-linux-client-$release_version.tar.gz
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
