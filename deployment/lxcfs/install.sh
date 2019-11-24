#!/bin/bash
mkdir -p /lib/systemd/system
mkdir -p /usr/local/lib/lxcfs
mkdir -p /var/lib/lxcfs
cp lxcfs.service /lib/systemd/system/lxcfs.service
cp lxcfs /usr/local/bin/lxcfs
cp -rf lib/* /usr/local/lib/lxcfs/
chmod  a+x lxcfs /usr/local/bin/lxcfs
systemctl enable lxcfs.service
systemctl restart lxcfs.service
