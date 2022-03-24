#!/bin/bash

# systemctl list-units --type service
# systemctl status qemu-guest-agent.service

# service --status-all | grep qemu
# service qemu-guest-agent status

# dbc@ubuntu-1804:~$ /etc/init.d/qemu-guest-agent status
# ● qemu-guest-agent.service - LSB: QEMU Guest Agent startup script
#    Loaded: loaded (/etc/init.d/qemu-guest-agent; generated)
#    Active: active (running) since Wed 2022-03-02 07:49:28 UTC; 3s ago
#      Docs: man:systemd-sysv-generator(8)
#   Process: 1366 ExecStop=/etc/init.d/qemu-guest-agent stop (code=exited, status=0/SUCCESS)
#   Process: 1384 ExecStart=/etc/init.d/qemu-guest-agent start (code=exited, status=0/SUCCESS)
#     Tasks: 1 (limit: 4915)
#    CGroup: /system.slice/qemu-guest-agent.service
#            └─1404 /usr/sbin/qemu-ga --daemonize -m virtio-serial -p /dev/virtio-ports/org.qemu.guest_agent.0

# Mar 02 07:49:28 ubuntu-1804 systemd[1]: Starting LSB: QEMU Guest Agent startup script...
# Mar 02 07:49:28 ubuntu-1804 systemd[1]: Started LSB: QEMU Guest Agent startup script.

# stop and remove qemu guest agent
sudo /etc/init.d/qemu-guest-agent stop
sudo rm -f /etc/init.d/qemu-guest-agent
sudo systemctl daemon-reload

sudo mv /usr/sbin/qemu-ga /usr/sbin/qemu-ga.bak

# download dbc-guest-agent
wget http://116.169.53.132:9000/dbc_guest_agent/dbc-ga
# wget http://116.169.53.132:9000/dbc_guest_agent/dbc-guest-agent
wget http://116.169.53.132:9000/dbc_guest_agent/dbc-guest-agent.service
sudo mv dbc-ga /usr/sbin/dbc-ga
sudo chmod 755 /usr/sbin/dbc-ga
# sudo mv dbc-guest-agent /etc/init.d/
# sudo chmod 755 /etc/init.d/dbc-guest-agent

# autostart
sudo mv dbc-guest-agent.service /lib/systemd/system/

sudo systemctl daemon-reload

# sudo /etc/init.d/dbc-guest-agent start
sudo systemctl enable dbc-guest-agent.service
sudo systemctl start dbc-guest-agent.service
echo "install dbc guest agent success"
