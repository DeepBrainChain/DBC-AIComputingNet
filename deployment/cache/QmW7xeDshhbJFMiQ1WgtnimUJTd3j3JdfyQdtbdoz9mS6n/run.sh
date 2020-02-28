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
run_jupyter()
{
    #JUPYTER_TOKEN=$(< /dev/urandom tr -dc A-Za-z0-9 | head -c64; echo)
    #nohup jupyter-notebook --ip 0.0.0.0 --port 8888 --no-browser --allow-root --NotebookApp.token=$JUPYTER_TOKEN &
    #URL_TOKEN="?token="$JUPYTER_TOKEN
    #echo "Jupyter_url_token:"$URL_TOKEN
    if [ "$GPU_SERVER_RESTART" == "yes" ]; then
        echo "keep jupyter current password"
    else
        JUPYTER_PASSWD=$(< /dev/urandom tr -dc A-Za-z0-9 | head -c8; echo)
        echo "JUPYTER_PASSWD:"$JUPYTER_PASSWD
        expect /chjupyter.exp $JUPYTER_PASSWD
    fi
    nohup jupyter notebook --ip 0.0.0.0 --port 8888 --no-browser --allow-root > jupyter.log 2>&1 &
}
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
            export GPU_SERVER_RESTART="yes"
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

start_nextcloud()
{
   if [ "$GPU_SERVER_RESTART" == "yes" ]; then
        echo "keep nextcloud current password"
    else
        NEXTCLOUD_PASSWD=$(< /dev/urandom tr -dc A-Za-z0-9 | head -c8; echo)
        echo "NEXTCLOUD_PASSWD:"$NEXTCLOUD_PASSWD
        expect /setNextcloudPwd.exp $NEXTCLOUD_PASSWD
    fi
    ip=$(curl ip.sb)
    sed -i "s/[0-9]\{1,3\}\.[0-9]\{1,3\}\.[0-9]\{1,3\}\.[0-9]\{1,3\}/$ip/g" /var/www/nextcloud/config/config.php
   # sed -i "s/ipaddress/$ip/g" /var/www/nextcloud/config/config.php
    service mysql start
    redis-server /etc/redis/redis.conf
    service apache2 restart


}

main_loop()
{
    # step 1
#    parse_arg

    # update .basrc
    append_to_bashrc "export IPFS_PATH=/dbc/.ipfs"
    append_to_bashrc "export LD_LIBRARY_PATH=/usr/local/nvidia/lib:/usr/local/nvidia/lib64"
    update_path "/opt/conda/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/opt/anaconda3/bin"
    update_path "/usr/local/nvidia/bin"

    source ~/.bashrc
    setup_ssh_service

    run_jupyter
    set_passwd
    start_nextcloud
     echo "support jupyter_lab"
     echo "support nextcloud"
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













