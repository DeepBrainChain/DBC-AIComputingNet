#!/bin/bash

id_pub=$1
token=$2
region=$3

echo "export IPFS_PATH=/dbc/.ipfs" >> ~/.bashrc

apt update >/dev/null
apt install openssh-server vim screen -y

TARGET_KEY="TCPKeepAlive"
REPLACEMENT_VALUE="no"
sed -i "s/\($TARGET_KEY * *\).*/\1$REPLACEMENT_VALUE/" /etc/ssh/sshd_config
/etc/init.d/ssh restart
mkdir -p ~/.ssh

echo ssh-rsa $id_pub >> ~/.ssh/authorized_keys
cd `dirname $0`/ng

sleep 5s
chmod +x ./ngrok
./ngrok authtoken $token
./ngrok tcp 22 --region=$region >/dev/null 2>&1 &
#./ngrok tcp 22 --region=$region >/dev/null &

echo "tcp port:"
for i in {1..60}; do
sleep 3s
port=$(curl localhost:4040/status 2>&1| grep "tcp://" | awk -F "URL" '{print $2}' | awk -F "," '{print $1}' | awk -F "tcp://" '{print $2}' | awk -F "\\" '{print $1}')
if [ -n "$port" ]; then
	echo $port
	break
fi
done

if [ ! -n $port ]; then
    curl localhost:4040/status 2>&1| grep tcp
	echo "fail to get the tcp port"
fi

while ! ls /tmp/bye &>/dev/null; do
   sleep 10s
done

echo "bye"