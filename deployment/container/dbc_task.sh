#!/bin/bash
#set -x

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




if [ ! -d /dbc ]; then
    mkdir /dbc
fi


# run ai-training task
/bin/bash /dbc_task_imp.sh "$1" "$2" $3

