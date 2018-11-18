#!/bin/bash
#set -x

config_ipfs()
{
    #add ipfs bootstrap node
    ipfs bootstrap rm --all

    #ipfs bootstrap add /ip4/114.116.19.45/tcp/4001/ipfs/QmPEDDvtGBzLWWrx2qpUfetFEFpaZFMCH9jgws5FwS8n1H
    ipfs bootstrap add /ip4/49.51.49.192/tcp/4001/ipfs/QmRVgowTGwm2FYhAciCgA5AHqFLWG4AvkFxv9bQcVB7m8c
    ipfs bootstrap add /ip4/49.51.49.145/tcp/4001/ipfs/QmPgyhBk3s4aC4648aCXXGigxqyR5zKnzXtteSkx8HT6K3
    ipfs bootstrap add /ip4/122.112.243.44/tcp/4001/ipfs/QmPC1D9HWpyP7e9bEYJYbRov3q2LJ35fy5QnH19nb52kd5

    wget https://github.com/DeepBrainChain/deepbrainchain-release/releases/download/latest/bootstrap_nodes
    cat ./bootstrap_nodes| while read line
    do
        ipfs bootstrap add $line
    done
    rm ./bootstrap_nodes

    #set ipfs repo
    ipfs config Datastore.StorageMax 100GB
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

echo "params: " $@
ipfs_tgz=go-ipfs_v0.4.15_linux-amd64.tar.gz

if [ $# -ne 3 ]; then
    echo "expect num of params as 3 but $#"
    exit
fi

# install necessary tool
if [ ! `which curl` ] || [ ! `which wget` ]; then
    echo "apt update"
    apt-get update
fi

if [ ! `which curl` ]; then
    echo "install curl"
    apt-get install --yes curl
fi

if [ ! `which wget` ]; then
    echo "install wget"
    apt-get install --yes wget
fi


# install ipfs repo to indicated directory, e.g. /dbc/
ipfs_install_path=/dbc/.ipfs

if [ ! -d /dbc ]; then
    mkdir /dbc
fi

wait_ipfs_daemon_ready=30s
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
        ipfs daemon --enable-gc &

        sleep $wait_ipfs_daemon_ready
        config_ipfs
        cd /
    else
        mkdir $ipfs_install_path
        bash ./install_ipfs.sh ./$ipfs_tgz

        #set ipfs repo
        ipfs config Datastore.StorageMax 100GB
    fi
else
    ipfs daemon --enable-gc &

    sleep $wait_ipfs_daemon_ready
    config_ipfs
    cd /
fi



code_dir=$2
task_id=""
if [ -f /home/dbc_utils/parameters ]; then
    source /home/dbc_utils/parameters
fi


echo "task start: $task_id"

# run ai-training task
/bin/bash /dbc_task_imp.sh "$1" "$code_dir" $3

echo "task completed: $task_id"

