#!/bin/bash
#set -x

#start_ipfs_daemon()
#{
#    USERID=`id -u`
#    PROC_NAME="ipfs"
#    ps_="ps -e -o uid -o pid -o command"
#    ipfs_pid=$($ps_  | grep [d]aemon | awk '{if (($1 == "'${USERID}'") && ($3~/'${PROC_NAME}'$/)) print $2}')
#    if [ -z "$ipfs_pid" ]; then
#        echo "ipfs daemon is starting"
#        nohup ipfs daemon --enable-gc >/dev/null 2>&1 &
#    else
#        # ipfs is running
#        return 0
#    fi
#
#    # wait process ready
#    sleep 1
#    ipfs_pid=$($ps_  | grep [d]aemon | awk '{if (($1 == "'${USERID}'") && ($3~/'${PROC_NAME}'$/)) print $2}')
#    if [ -z "$ipfs_pid" ]; then
#        echo "error: fail to start ipfs daemon"
#        return 1
#    else
#        # loop until ipfs daemon is ready for use
#        for i in {1..60}; do
#            echo -n "."
#            if ipfs swarm peers &>/dev/null; then
#                echo
#                echo "ipfs is started"
#                return 0
#            fi
#            sleep 1
#        done
#    fi
#
#    echo
#    echo "error: fail to start ipfs daemon!"
#    return 1
#}
#
#setup_ipfs()
#{
#    # install ipfs repo to indicated directory, e.g. /dbc/
#    ipfs_install_path=/dbc/.ipfs
#
#    export IPFS_PATH=$ipfs_install_path
#
#    if [ "$restart" != "true" ]; then
#        cp -f /home/dbc_utils/$ipfs_tgz /
#        # install ipfs repo
#
#        mkdir $ipfs_install_path
#        bash ./install_ipfs.sh ./$ipfs_tgz
#
#        #set ipfs repo
#        ipfs config Datastore.StorageMax 100GB
#
#    else
#        if ! start_ipfs_daemon; then
#                echo "task completed: $task_id"
#                exit 1
#        fi
#        cd /
#    fi
#}


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

#setup_ipfs

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

