#!/bin/bash

DIR=$(cd $( dirname "${BASH_SOURCE[0]}" ) >/dev/null && pwd)
count=$(grep www.dbctalk.ai /etc/hosts|wc -l)

if [ $count -eq 0 ]
then
    echo "111.44.254.135   www.dbctalk.ai" >> /etc/hosts
fi
repo_name="www.dbctalk.ai"
repo_port=5000
repo_crt=$DIR/domain.crt

echo "register private repo $repo_name"
if [ -d /etc/docker/certs.d/$repo_name:$repo_port ]
then
    rm -r /etc/docker/certs.d/$repo_name:$repo_port
fi
sudo mkdir -p /etc/docker/certs.d/$repo_name:$repo_port
sudo cp $repo_crt /etc/docker/certs.d/$repo_name:$repo_port/
