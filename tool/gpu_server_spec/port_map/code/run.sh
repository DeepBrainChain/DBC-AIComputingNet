#!/bin/bash

args="$@"
echo $args


#parse_arg()
#{
#    test -f ./jq && chmod +x ./jq
#
#    debug=($(echo $args | ./jq -r .debug?))
#
#    if [ "$debug" == "true" ]; then
#        set -x
#    fi
#}

setup_ssh_service()
{
    if ! which sshd
    then
        echo "apt update"
        apt update >/dev/null
        echo "apt install openssh-server vim screen"
        apt install openssh-server vim screen -y >/dev/null
    fi


    TARGET_KEY="TCPKeepAlive"
    REPLACEMENT_VALUE="no"
    sed -i "s/\($TARGET_KEY * *\).*/\1$REPLACEMENT_VALUE/" /etc/ssh/sshd_config

    sed  -i "s/prohibit-password/yes/" /etc/ssh/sshd_config


    sleep 2s
    if ! service ssh status; then
        service ssh start
    else
        service ssh restart
    fi

    # allow ssh through ssh-rsa key
    mkdir -p ~/.ssh
    if [ "$id_pub" != "null" ]; then
        echo ssh-rsa $id_pub >> ~/.ssh/authorized_keys
    fi
}

set_passwd()
{
    # check environment variable set by parent shell script
    if [ "$GPU_SERVER_RESTART" == "yes" ]; then
        echo "keep current password"
    else
        default_pwd=$(< /dev/urandom tr -dc A-Za-z0-9 | head -c8; echo)
        if [ "$default_pwd" != "null" ]; then
            echo root:$default_pwd|chpasswd
            echo "pwd: $default_pwd"

        else
            echo "fail to set passwword"
        fi
    fi
}

append_to_bashrc()
{
	s=$1
	if ! grep "$1" ~/.bashrc; then
		echo "$1" >> ~/.bashrc
	fi
}

update_path()
{
    path=$1
	if ! grep PATH ~/.bashrc | grep $1; then
        echo "export PATH=$PATH:$1" >> ~/.bashrc
	fi
}

main_loop()
{
    # step 1
#    parse_arg

    # update .basrc
    append_to_bashrc "export IPFS_PATH=/dbc/.ipfs"
    append_to_bashrc "export LD_LIBRARY_PATH=/usr/local/nvidia/lib:/usr/local/nvidia/lib64"
    update_path "/opt/conda/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin"
    update_path "/usr/local/nvidia/bin"


    setup_ssh_service

    set_passwd


    echo "gpu server is ready"

    rm -rf /tmp/bye

    # loop until say bye
    while ! ls /tmp/bye &>/dev/null; do
       sleep 10s
       if ! service ssh status >/dev/null;then
            echo "start sshd"
            service ssh start
       fi
    done

    echo "bye"
    rm -f /tmp/bye
}

main_loop













