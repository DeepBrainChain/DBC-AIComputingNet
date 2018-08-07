#!/bin/bash
#set -x

release_version=0.3.3.1

echo "begin to wget DBC release package"
wget https://github.com/DeepBrainChain/deepbrainchain-release/releases/download/0.3.3.1/dbc-linux-client-0.3.3.1.tar.gz
if [ $? -ne 0 ]; then
    echo "***wget DBC release package failed***"
    exit
fi
tar -zxvf dbc-linux-client-0.3.3.1.tar.gz
echo "wget DBC release package finished"
echo -e

cd ./$release_version
current_directory=`pwd`
echo "current directory is $current_directory "

echo "start to install ipfs"
cd ./ipfs_repo/
/bin/bash ./install_ipfs.sh go-ipfs_v0.4.15_linux-amd64.tar.gz
cd ./../
echo "start install ipfs finished"
echo -e


echo "begin to start DBC program"
cd ./dbc_repo/
./dbc
