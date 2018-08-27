#!/bin/bash
#set -x

release_version=0.3.4.0

echo "begin to wget DBC release package"
curl -O -L https://github.com/DeepBrainChain/deepbrainchain-release/releases/download/0.3.4.0/dbc-macos-client-0.3.4.0.tar.gz
if [ $? -ne 0 ]; then
    echo "***curl DBC release package failed***"
    exit
fi
tar -zxvf dbc-macos-client-0.3.4.0.tar.gz
rm -rf dbc-macos-client-0.3.4.0.tar.gz
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

echo "begin to add dbc executable path to ENV PATH"
echo "export DBC_PATH=$current_directory/dbc_repo" >> ~/.bashrc
echo 'export PATH=$PATH:$DBC_PATH' >> ~/.bashrc
echo "export DBC_TOOL_PATH=$current_directory/dbc_repo/tool" >> ~/.bashrc 
echo 'export PATH=$PATH:$DBC_TOOL_PATH' >> ~/.bashrc
echo "source .bashrc" >> ~/.bash_profile
echo -e

echo "dbc ENV installation finished,source ~/.bashrc and run dbc to start"
