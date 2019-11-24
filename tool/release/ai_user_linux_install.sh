#!/bin/bash
#set -x

release_version=0.3.4.1

echo "begin to wget DBC release package"
wget https://github.com/DeepBrainChain/deepbrainchain-release/releases/download/latest/dbc-linux-client-$release_version.tar.gz
if [ $? -ne 0 ]; then
    echo "***wget DBC release package failed***"
    exit
fi
tar -zxvf dbc-linux-client-$release_version.tar.gz
rm -rf dbc-linux-client-$release_version.tar.gz
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

echo "start to install lxcfs"
cd ./lxcfs/
/bin/bash ./install.sh
cd ./../
echo "install lxcfs finished"
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

