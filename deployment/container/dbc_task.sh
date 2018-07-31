#!/bin/bash
#set -x

# cp utility
cp -f /home/dbc_utils/dbc_task_imp.sh /
cp -f /home/dbc_utils/swarm.key /
cp -f /home/dbc_utils/install_ipfs.sh /

cp /home/dbc_utils/upload.sh /usr/bin/
cp /home/dbc_utils/dos2unix /usr/bin/

chmod +x /usr/bin/upload.sh
chmod +x /usr/bin/dos2unix

cd /

echo "params: " $@

if [ $# -ne 3 ]; then
    echo "expect num of params as 3 but $#"
    exit
fi

# install ipfs repo to indicated directory, e.g. /dbc/
ipfs_install_path=/dbc/.ipfs

if [ ! -d /dbc ]; then
    mkdir /dbc
fi
mkdir $ipfs_install_path

export IPFS_PATH=$ipfs_install_path

# install ipfs repo
cp -f /home/dbc_utils/go-ipfs_v0.4.15_linux-amd64.tar.gz /

bash ./install_ipfs.sh ./go-ipfs_v0.4.15_linux-amd64.tar.gz force $ipfs_install_path

# install curl
if [ ! `which curl` ]; then
    echo "install curl"
    apt-get update
    apt-get install --yes curl
fi

# run ai-training task
/bin/bash /dbc_task_imp.sh "$1" "$2" $3

