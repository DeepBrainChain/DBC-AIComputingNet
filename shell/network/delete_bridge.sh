#!/bin/bash

DHCPNS=$1
VXLAN_NAME=$2
BRIDGE_NAME=$1

ip link del ${VXLAN_NAME}
ip link del ${BRIDGE_NAME}
