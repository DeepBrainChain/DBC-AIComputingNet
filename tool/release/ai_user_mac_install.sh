#!/bin/bash
#set -x

release_version=0.3.4.1

echo "begin to wget DBC release package"
curl -O -L https://github.com/DeepBrainChain/deepbrainchain-release/releases/download/latest/dbc-macos-client-$release_version.tar.gz
if [ $? -ne 0 ]; then
    echo "***curl DBC release package failed***"
    exit
fi
tar -zxvf dbc-macos-client-$release_version.tar.gz
rm -rf dbc-macos-client-$release_version.tar.gz
echo "curl DBC release package finished"
echo -e

cd ./$release_version
current_directory=`pwd`
echo "current directory is $current_directory "

echo "start to install ipfs"
cd ./ipfs_repo/
bash ./install_ipfs.sh ./go-ipfs_v0.4.15_darwin-amd64.tar.gz
cd ./../
echo "start install ipfs finished"
echo -e

echo "show node id:"
cd ./dbc_repo/
./dbc --id
cd ./../
echo -e

echo "begin to add dbc executable path to ENV PATH"
cat ~/.bashrc | grep DBC_PATH
if [ $? -ne 0 ]; then
    echo "export DBC_PATH=$current_directory/dbc_repo" >> ~/.bashrc    
    echo 'export PATH=$PATH:$DBC_PATH' >> ~/.bashrc
    echo 'export PATH=$PATH:$DBC_PATH/tool' >> ~/.bashrc
    echo "source .bashrc" >> ~/.bash_profile
else
    sed -i "/dbc_repo/c export DBC_PATH=$current_directory/dbc_repo" ~/.bashrc
fi
echo -e

echo "dbc ENV installation finished,source ~/.bashrc and run dbc to start"

