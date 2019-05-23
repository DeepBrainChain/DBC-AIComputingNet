#!/bin/bash
#set -x

if [ $# -ne 1 ]; then
        echo "params count is 1, please check params error"
        exit
fi

USER_NAME=$1

if id $USER_NAME > /dev/null ; then
    echo "user $USER_NAME already exists"
    exit 1
fi

groupadd $USER_NAME
useradd -g $USER_NAME -d /home/$USER_NAME -s /bin/bash -m $USER_NAME


passwd $USER_NAME
echo "$USER_NAME    ALL=(ALL:ALL) ALL" >> /etc/sudoers
