#!/bin/bash

VTEP=$(ip route|awk 'NR==1{print $5}')
BRIDGE_NAME=$1
VXLAN_NAME=$2
VXLAN_VNI=$3

sudo ip link add ${VXLAN_NAME} type vxlan vni ${VXLAN_VNI} group 239.0.0.8 dev ${VTEP} df inherit dstport 4789 >/dev/null 2>&1
if [ $? -ne 0 ];then
    printf "{\"errcode\":-1,\"message\":\"create vxlan failed\"}\n"
    exit 1
fi
sudo brctl addbr ${BRIDGE_NAME} >/dev/null 2>&1
if [ $? -ne 0 ];then
    printf "{\"errcode\":-1,\"message\":\"create bridge failed\"}\n"
    exit 1
fi
sudo ip link set ${VXLAN_NAME} up >/dev/null 2>&1
if [ $? -ne 0 ];then
    printf "{\"errcode\":-1,\"message\":\"up vxlan failed\"}\n"
    exit 1
fi
sudo ip link set ${BRIDGE_NAME} up >/dev/null 2>&1
if [ $? -ne 0 ];then
    printf "{\"errcode\":-1,\"message\":\"up bridge failed\"}\n"
    exit 1
fi
sudo brctl addif ${BRIDGE_NAME} ${VXLAN_NAME} >/dev/null 2>&1
if [ $? -ne 0 ];then
    printf "{\"errcode\":-1,\"message\":\"add bridge to vxlan failed\"}\n"
    return 1
fi

printf "{\"errcode\":0,\"message\":\"ok\"}\n"
