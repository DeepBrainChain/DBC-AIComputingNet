#!/bin/bash

VTEP=$(ip route|awk 'NR==1{print $5}')
BRIDGE_NAME=$1
VXLAN_NAME=$2
VXLAN_VNI=$3

sudo ip link add ${VXLAN_NAME} type vxlan vni ${VXLAN_VNI} group 239.0.0.8 dev ${VTEP} df inherit dstport 4789
sudo brctl addbr ${BRIDGE_NAME}
sudo ip link set ${VXLAN_NAME} up
sudo ip link set ${BRIDGE_NAME} up 
sudo brctl addif ${BRIDGE_NAME} ${VXLAN_NAME}
