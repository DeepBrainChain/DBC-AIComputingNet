#!/bin/sh

WORKDIR=$( cd $( dirname $0 ) && pwd )
cd $WORKDIR

if [ $# -lt 5 ]
then
	echo "Usage:`basename $0`  host  ssh_port  username  passwd  remote_dir"
	exit 1
fi

HOST=$1
PORT=$2
USERNAME=$3
PASSWD=$4

. ./common_func.sh

remote_command ${HOST} ${PORT} ${USERNAME} ${PASSWD} "ls -l $5 |grep ^- |awk -F' ' '{print \$9}' |paste -s -d, |tr -d '[[:space:]]'"

