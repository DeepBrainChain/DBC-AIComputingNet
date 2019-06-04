#!/bin/bash

DIR=$(cd $( dirname "${BASH_SOURCE[0]}" ) >/dev/null && pwd)

repo_name="hkksros01.3322.org"
repo_port=5000
repo_crt=$DIR/domain.crt

echo "register private repo $repo_name"

sudo mkdir -p /etc/docker/certs.d/$repo_name:$repo_port
sudo cp $repo_crt /etc/docker/certs.d/$repo_name:$repo_port/

