#!/bin/sh

# ip ssh_port username pwd cmd
remote_command ()
{
	ip=${1}
    port=${2}
    username=${3}
	pwd=${4}
    cmd=${5}
	/usr/bin/expect do_command.exp ${ip} ${port} ${username} ${pwd} "${cmd}" | grep --line-buffered -v spawn | grep --line-buffered -v password
}

