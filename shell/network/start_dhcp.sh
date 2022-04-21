#!/bin/bash

DHCPNS=$1
BRIDGE_NAME=$1
VXLAN_NAME=$2
DHCPNAME_BR=$3
DHCPNAME_NS="${DHCPNAME_BR}-0"
IP_START=$5
IP_END=$6
NETMASK=$4
IP_LEASE_NUM=$7

ip netns add ${DHCPNS} >/dev/null 2>&1
if [ $? -ne 0 ];then
    printf "{\"errcode\":-1,\"message\":\"add netns failed\"}\n"
    exit 1
fi

ip link add ${DHCPNAME_BR} type veth peer name ${DHCPNAME_NS} >/dev/null 2>&1
if [ $? -ne 0 ];then
    printf "{\"errcode\":-1,\"message\":\"add veth pair failed\"}\n"
    ip link del ${DHCPNAME_BR} >/dev/null 2>&1
    exit 1
fi

ip link set ${DHCPNAME_NS} netns ${DHCPNS} >/dev/null 2>&1
if [ $? -ne 0 ];then
    printf "{\"errcode\":-1,\"message\":\"add veth into netns failed\"}\n"
    exit 1
fi

ip netns exec ${DHCPNS} ip address add ${IP_START}/${NETMASK} dev ${DHCPNAME_NS} >/dev/null 2>&1
if [ $? -ne 0 ];then
    printf "{\"errcode\":-1,\"message\":\"netns set veth ip address failed\"}\n"
    exit 1
fi

ip netns exec ${DHCPNS} ip link set lo up >/dev/null 2>&1
if [ $? -ne 0 ];then
    printf "{\"errcode\":-1,\"message\":\"netns up lo failed\"}\n"
    exit 1
fi

ip netns exec ${DHCPNS} ip link set ${DHCPNAME_NS} mtu 1450 up >/dev/null 2>&1
if [ $? -ne 0 ];then
    printf "{\"errcode\":-1,\"message\":\"netns set veth mtu failed\"}\n"
    exit 1
fi

sudo brctl addif ${BRIDGE_NAME} ${DHCPNAME_BR} >/dev/null 2>&1
if [ $? -ne 0 ];then
    printf "{\"errcode\":-1,\"message\":\"add netns into bridge failed\"}\n"
    exit 1
fi

ip link set ${DHCPNAME_BR} up >/dev/null 2>&1
if [ $? -ne 0 ];then
    printf "{\"errcode\":-1,\"message\":\"up veth failed\"}\n"
    exit 1
fi

MESSAGE=$(ip netns exec ${DHCPNS} sh -c '/usr/sbin/dnsmasq --except-interface lo --bind-interfaces --interface '${DHCPNAME_NS}' --port=0 --pid-file=/var/run/'${VXLAN_NAME}'.pid --dhcp-range '${IP_START}','${IP_END}','${NETMASK}',10h --dhcp-no-override --dhcp-leasefile=/var/lib/misc/'${DHCPNS}'.leases --dhcp-lease-max='${IP_LEASE_NUM}' --dhcp-option=3' 2>&1)
if [ $? -ne 0 ];then
    printf "{\"errcode\":-1,\"message\":\"${MESSAGE}\"}\n"
    exit 1
fi

printf "{\"errcode\":0,\"message\":\"The pid path is:/var/run/${VXLAN_NAME}\"}\n"
