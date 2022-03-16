#!/bin/bash

VTEP=$(ip route|awk 'NR==1{print $5}')
BRIDGE_NAME=$1
VXLAN_NAME=$2
VXLAN_VNI=$3
DHCPNS=$1
DHCPNAME_BR="tap$(openssl rand -hex 20 | cut -c 1-10)"
DHCPNAME_NS="${DHCPNAME_BR}-0"
#PREFIX=$6
IP_CIDR=$4
GATEWAY_IP=''
NETMASK=''
IP_LEASE_NUM=''
PREFIX=$(echo $4|awk -F "/" '{print $2}')
IP_SEGMENT=$(echo $4|awk -F "/" '{print $1}')
IP_START=$5
IP_END=$6
MESSAGE=''
ABC=''


function create_bridge()
{
sudo ip link add ${VXLAN_NAME} type vxlan vni ${VXLAN_VNI} group 239.0.0.8 dev ${VTEP} df inherit dstport 4789
sudo brctl addbr ${BRIDGE_NAME}
sudo ip link set ${VXLAN_NAME} up
sudo ip link set ${BRIDGE_NAME} up 
sudo brctl addif ${BRIDGE_NAME} ${VXLAN_NAME}
}

function get_ip()
{
IP1=$(echo $1|awk -F "." '{print $1}')
IP2=$(echo $1|awk -F "." '{print $2}')
IP3=$(echo $1|awk -F "." '{print $3}')
IP4=$(echo $1|awk -F "." '{print $4}')
bit=$((32-$2))
tmp=$((4294967295<<${bit}))
sub4=$((${tmp} &255))
sub3=$((${tmp}>>8 &255))
sub2=$((${tmp}>>16 &255))
sub1=$((${tmp}>>24 &255))
gwip1=$((${IP1}&${sub1}))
gwip2=$((${IP2}&${sub2}))
gwip3=$((${IP3}&${sub3}))
gwip4=$(($((${IP4}&${sub4}))+1))

GATEWAY_IP=${gwip1}.${gwip2}.${gwip3}.${gwip4}
NETMASK=${sub1}.${sub2}.${sub3}.${sub4}
IP_LEASE_NUM=$((2**${bit}-2))
}


function create_dhcp()
{
ip netns add ${DHCPNS}
ip link add ${DHCPNAME_BR} type veth peer name ${DHCPNAME_NS}
ip link set ${DHCPNAME_NS} netns ${DHCPNS}
ip netns exec ${DHCPNS} ip address add ${GATEWAY_IP}/${NETMASK} dev ${DHCPNAME_NS}
ip netns exec ${DHCPNS} ip link set lo up
ip netns exec ${DHCPNS} ip link set ${DHCPNAME_NS} mtu 1450 up
sudo brctl addif ${BRIDGE_NAME} ${DHCPNAME_BR}
ip link set ${DHCPNAME_BR} up
MESSAGE=$(ip netns exec ${DHCPNS} sh -c 'dnsmasq --except-interface lo --bind-interfaces --interface '${DHCPNAME_NS}' --port=0 --pid-file=/var/run/'${VXLAN_NAME}'.pid --dhcp-range '${GATEWAY_IP}','${IP_END}','${NETMASK}',10h --dhcp-no-override --dhcp-leasefile=/var/lib/misc/'${DHCPNS}'.leases --dhcp-lease-max='${IP_LEASE_NUM}' --dhcp-option=3' 2>&1)
return $?
#ABC=${MESSAGE}
#echo hanshulimina:$?
#if [ $? -ne 0 ];then
#	return 1
#fi
}
get_ip ${IP_SEGMENT} ${PREFIX}
create_bridge
create_dhcp
if [ $? -eq 0 ];then
	printf "{\"errcode\":$?,\"message\":\"The pid path is:/var/run/${VXLAN_NAME}\"}\n"
else
	printf "{\"errcode\":-1,\"message\":\"${MESSAGE}\"}\n"
fi
