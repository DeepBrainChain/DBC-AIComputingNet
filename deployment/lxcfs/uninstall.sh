#!/bin/bash
systemctl stop lxcfs.service
systemctl disable lxcfs.service
rm -rf /usr/local/bin/lxcfs
rm -rf /usr/local/lib/lxcfs
rm -rf /var/lib/lxcfs
