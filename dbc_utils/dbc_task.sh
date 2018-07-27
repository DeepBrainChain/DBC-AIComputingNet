#!/bin/bash
#set -x

# cp utility
cp -f /home/dbc_utils/dbc_task_imp.sh /
cp -f /home/dbc_utils/swarm.key /
cp /home/dbc_utils/upload.sh /usr/bin/
cp /home/dbc_utils/dos2unix /usr/bin/

chmod +x /usr/bin/upload.sh
chmod +x /usr/bin/dos2unix

cd /

echo "params: " $@

chmod +x /dbc_task_imp.sh

if [ $# -ne 3 ]; then
    echo "expect num of params as 3 but $#"
    exit
fi

# install ipfs and ipfsapi
echo "install ipfs"
cp -f /home/dbc_utils/go-ipfs_v0.4.15_linux-amd64.tar.gz /opt/
cd /opt
tar xvzf ./go-ipfs_v0.4.15_linux-amd64.tar.gz
cd /opt/go-ipfs
./install.sh

cd /
rm -rf /opt/go-ipfs

echo "install ipfsapi"
pip install ipfsapi


# install curl
if [ ! `which curl` ]; then
    echo "install curl"
    apt-get update
    apt-get install --yes curl
fi

# run ai-training task
/bin/bash /dbc_task_imp.sh "$1" "$2" $3

