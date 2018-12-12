#!/bin/bash
#set -x

config_ipfs()
{
    #add ipfs bootstrap node
    ipfs bootstrap rm --all >/dev/null

    ipfs bootstrap add /ip4/122.112.243.44/tcp/4001/ipfs/QmPC1D9HWpyP7e9bEYJYbRov3q2LJ35fy5QnH19nb52kd5 >/dev/null
    ipfs bootstrap add /ip4/18.223.4.215/tcp/4001/ipfs/QmeZR4HygPvdBvheovYR4oVTvaA4tWxDPTgskkPWqbjkGy >/dev/null

    if wget -quiet https://github.com/DeepBrainChain/deepbrainchain-release/releases/download/latest/bootstrap_nodes &>/dev/null; then
        echo "add ipfs node from dbc website"

        cat ./bootstrap_nodes| while read line
        do
            ipfs bootstrap add $line >/dev/null
        done
        rm ./bootstrap_nodes
    fi

    #set ipfs repo
    ipfs config Datastore.StorageMax 100GB
}

start_ipfs_daemon()
{
    USERID=`id -u`
    PROC_NAME="ipfs"
    ps_="ps -e -o uid -o pid -o command"
    ipfs_pid=$($ps_  | grep [d]aemon | awk '{if (($1 == "'${USERID}'") && ($3~/'${PROC_NAME}'$/)) print $2}')
    if [ -z "$ipfs_pid" ]; then
        echo "ipfs daemon is starting"
        nohup ipfs daemon --enable-gc >/dev/null 2>&1 &
    else
        # ipfs is running
        return 0
    fi

    # wait process ready
    sleep 1
    ipfs_pid=$($ps_  | grep [d]aemon | awk '{if (($1 == "'${USERID}'") && ($3~/'${PROC_NAME}'$/)) print $2}')
    if [ -z "$ipfs_pid" ]; then
        echo "error: fail to start ipfs daemon"
        return 1
    else
        # loop until ipfs daemon is ready for use
        for i in {1..60}; do
            echo -n "."
            if ipfs swarm peers &>/dev/null; then
                echo
                echo "ipfs is started"
                return 0
            fi
            sleep 1
        done
    fi

    echo
    echo "error: fail to start ipfs daemon!"
    return 1
}


# cp utility
cp -f /home/dbc_utils/dbc_task.sh /
cp -f /home/dbc_utils/dbc_task_imp.sh /
cp -f /home/dbc_utils/swarm.key /
cp -f /home/dbc_utils/install_ipfs.sh /

cp /home/dbc_utils/dbc_upload /usr/bin/
cp /home/dbc_utils/dos2unix /usr/bin/

chmod +x /usr/bin/dbc_upload
chmod +x /usr/bin/dos2unix

cd /


ipfs_tgz=go-ipfs_v0.4.15_linux-amd64.tar.gz

if [ $# -ne 3 ]; then
    echo "expect num of params as 3 but $#"
    exit
fi

# install necessary tool
if [ ! `which curl` ] || [ ! `which wget` ]; then
    echo "apt update"
    apt-get update >/dev/null
fi

if [ ! `which curl` ]; then
    echo "install curl"
    apt-get install --yes curl >/dev/null
fi

if [ ! `which wget` ]; then
    echo "install wget"
    apt-get install --yes wget >/dev/null
fi


code_dir=$2
task_id=""
if [ -f /home/dbc_utils/parameters ]; then
    source /home/dbc_utils/parameters
fi

echo "task start: $task_id"
echo "params: " $@

# install ipfs repo to indicated directory, e.g. /dbc/
ipfs_install_path=/dbc/.ipfs

if [ ! -d /dbc ]; then
    mkdir /dbc
fi

rm -rf /dbc/*

export IPFS_PATH=$ipfs_install_path

if [ ! `which ipfs` ]; then
    cp -f /home/dbc_utils/$ipfs_tgz /
    # install ipfs repo
    if [ -d /dbc/.ipfs ]; then
        echo "reuse ipfs cache"
        cp -f $ipfs_tgz /tmp
        cd /tmp
        tar xvzf $ipfs_tgz >/dev/null
        cd /tmp/go-ipfs
        cp /tmp/go-ipfs/ipfs /usr/local/bin/

        if ! start_ipfs_daemon; then
            echo "task completed: $task_id"
            exit 1
        fi

        config_ipfs
        cd /
    else
        mkdir $ipfs_install_path
        bash ./install_ipfs.sh ./$ipfs_tgz

        #set ipfs repo
        ipfs config Datastore.StorageMax 100GB
    fi
else

    if ! start_ipfs_daemon; then
            echo "task completed: $task_id"
            exit 1
    fi

    config_ipfs
    cd /
fi


# run ai-training task
/bin/bash /dbc_task_imp.sh "$1" "$code_dir" $3

echo "task completed: $task_id"

