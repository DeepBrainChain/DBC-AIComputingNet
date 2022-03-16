#!/bin/bash

DHCPNS=$1
VXLAN_NAME=$2
BRIDGE_NAME=$1

ip link del ${VXLAN_NAME}
ip link del ${BRIDGE_NAME}
ip netns list |grep "${DHCPNS}" > /dev/null 2>&1
#if [ "${DHCPNS}" != '' ];then
if [ $? -eq 0 ];then
ip netns del ${DHCPNS}
fi
kill `cat /var/run/${VXLAN_NAME}.pid`
if [ $? -eq 0 ];then
	rm -f /var/run/${VXLAN_NAME}.pid
fi


