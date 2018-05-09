#!/bin/bash
#set -x

if [ $# -ne 1 ]; then
	echo "params count is 1, please check params error"
	exit
fi

USER_NAME=$1
groupadd $USER_NAME
useradd -g $USER_NAME -d /home/$USER_NAME $USER_NAME
mkdir /home/$USER_NAME
chown $USER_NAME:$USER_NAME /home/$USER_NAME
passwd $USER_NAME
echo "$USER_NAME    ALL=(ALL:ALL) ALL" >> /etc/sudoers

