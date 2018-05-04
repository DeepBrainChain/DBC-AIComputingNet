#!/bin/bash
# while 1000.
x=0
while [ $x -ne 60000 ]
do
echo "peer=192.168.100.144:21107" >> peer.conf
x=$(($x+1 ))
done
