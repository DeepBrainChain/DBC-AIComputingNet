#!/bin/bash

# cp utility
cp -f /home/dbc_utils/dbc_task.sh /
cp -f /home/dbc_utils/dbc_task_imp.sh /
#cp -f /home/dbc_utils/swarm.key /
#cp -f /home/dbc_utils/install_ipfs.sh /

cp /home/dbc_utils/dbc_upload /usr/bin/
cp /home/dbc_utils/dos2unix /usr/bin/

cp -f /home/dbc_utils/jq /usr/bin/
cp -f /home/dbc_utils/ngrok /usr/bin/

chmod +x /usr/bin/dbc_upload
chmod +x /usr/bin/dos2unix
chmod +x /usr/bin/jq
chmod +x /usr/bin/ngrok

cd /


#ipfs_tgz=go-ipfs_v0.4.15_linux-amd64.tar.gz

if [ $# -ne 3 ]; then
    echo "expect num of params as 3 but $#"
    exit
fi

# install necessary tool
if [ -f /etc/centos-release ]; then
    echo "install common tools for centos"
    yum -y install curl wget vim openssl openssh-server passwd net-tools &>/dev/null
else
    echo "install common tools for ubutu"
    apt-get update
    apt-get install --yes wget curl >/dev/null
fi

restart=false
code_dir=$2
task_id=""
if [ -f /home/dbc_utils/parameters ]; then
    source /home/dbc_utils/parameters
fi

echo "task start: $task_id"
echo "params: " $@


if [ ! -d /dbc ]; then
    mkdir /dbc
fi

if [ "$restart" != "true" ]; then
  rm -rf /dbc/*
fi


# run ai-training task
if [ "$restart" == "true" ]; then
    export GPU_SERVER_RESTART="yes"
    /bin/bash /dbc_task_imp.sh "$restart" "$code_dir" $3
else
    /bin/bash /dbc_task_imp.sh "$1" "$code_dir" $3
fi

echo "task completed: $task_id"

