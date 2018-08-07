#!/bin/bash


if [ $# -lt 1 ]; then
    echo "bash install_ipfs.sh [go-ipfs.tgz]"
    [[ "$0" = "$BASH_SOURCE" ]] && exit 1 || return 1
fi


workspace=`pwd`
sleep_time=5s
wait_ipfs_init_time=10s
wait_ipfs_daemon_time=30s
ipfs_tgz=$1

# env IPFS_PATH, as ipfs product doc required. by default ~/.ipfs
ipfs_path=$IPFS_PATH
if [ -z "$ipfs_path" ]; then
    ipfs_path=~/.ipfs
fi

rm -rf $ipfs_path
mkdir $ipfs_path


# install ipfs
echo "install ipfs"
cp -f $ipfs_tgz /tmp
cd /tmp
tar xvzf $ipfs_tgz
cd /tmp/go-ipfs

user=`whoami`
if [ "$user" != "root" ]; then
    if [ ! -d "/usr/local/bin" ]; then
        sudo mkdir -p /usr/local/bin
    fi
    sudo ./install.sh
else
    if [ ! -d "/usr/local/bin" ]; then
        mkdir -p /usr/local/bin
    fi
    ./install.sh
fi

cd $workspace
rm -rf /tmp/go-ipfs

#init ipfs
echo "begin to init ipfs"
ipfs init &
sleep $wait_ipfs_init_time

exit_code=$?

if [ $exit_code -ne 0 ]; then
        echo "init ipfs failed"
        exit
fi

echo "end to init ipfs"
sleep $sleep_time


cp ./swarm.key $ipfs_path/

#start ipfs
#echo "======================================================="
echo "begin to configure ipfs"
ipfs daemon --enable-gc &
sleep $wait_ipfs_daemon_time

exit_code=$?
#echo -n "start ipfs exitcode:"
#echo $exit_code

if [ $exit_code -ne 0 ]; then
        echo "configure ipfs failed"
        exit
fi

#add ipfs bootstrap node
ipfs bootstrap rm --all

wget https://github.com/DeepBrainChain/deepbrainchain-release/releases/download/0.3.3.1/bootstrap_nodes
cat ./bootstrap_nodes| while read line
do
  ipfs bootstrap add $line
done

rm ./bootstrap_nodes


echo "ipfs configure complete"

sleep $sleep_time
