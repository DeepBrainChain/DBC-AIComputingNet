#!/bin/bash

args="$@"
echo $args


parse_arg()
{
    test -f ./jq && chmod +x ./jq

    # format check
    if ! echo $args | ./jq -r .debug? ; then
        echo "json format error: $args"
        exit 1
    fi

    debug=($(echo $args | ./jq -r .debug?))

    if [ "$debug" == "true" ]; then
        set -x
    fi

    # mandatory
    server_ip=($(echo $args | ./jq -r .dns_server[].ip?))

}

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

    ps -ef|grep jupyter-lab | awk '{print  $2}'| sudo xargs  kill -9

    nohup jupyter-lab --ip 0.0.0.0 --port 8888 --no-browser --allow-root &
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
         #   export GPU_SERVER_RESTART="yes"
        else
            echo "fail to set passwword"
        fi
    fi
}

append_to_bashrc()
{

	if ! grep "$1" ~/.bashrc; then
		echo "$2" >> ~/.bashrc
	fi
}

update_path()
{

	if ! grep $1 ~/.bashrc; then
        echo "export PATH=\$PATH:$1" >> ~/.bashrc
	fi
}

start_nextcloud()
{

  #  ip=$(curl ip.sb)
    ip=${server_ip}
    sed -i "s/0 => '[0-9]\{1,3\}\.[0-9]\{1,3\}\.[0-9]\{1,3\}\.[0-9]\{1,3\}/0 => '$ip/g" /var/www/nextcloud/config/config.php
    sed -i "s#http://[0-9]\{1,3\}\.[0-9]\{1,3\}\.[0-9]\{1,3\}\.[0-9]\{1,3\}#http://$ip#g" /var/www/nextcloud/config/config.php
   # sed -i "s/ipaddress/$ip/g" /var/www/nextcloud/config/config.php

    service mysql restart
    sleep 10s
    redis-server /etc/redis/redis.conf
    sudo rm /var/run/apache2/apache2.pid
    ps -ef|grep apache2 | awk '{print $2}' | sudo xargs kill -9

    service apache2 restart

    if [ "$GPU_SERVER_RESTART" == "yes" ]; then
        echo "keep nextcloud current password"
    else
        sleep 20s
        NEXTCLOUD_PASSWD=$(< /dev/urandom tr -dc A-Za-z0-9 | head -c8; echo)

        expect /setNextcloudPwd.exp $NEXTCLOUD_PASSWD
        echo "NEXTCLOUD_PASSWD:"$NEXTCLOUD_PASSWD

    fi

}


auto_scan_nextcloud()
{
    if [ "$GPU_SERVER_RESTART" == "yes" ]; then
       echo "autoshell has been created"
    else
        /etc/init.d/cron restart
        if [  -f "/autoshell/scan.cron" ]; then
            rm /autoshell/scan.cron
        fi
        touch /autoshell/scan.cron
        echo '*/1 * * * *  sh /autoshell/scan_nextcloud.sh' >> /autoshell/scan.cron

    fi

    service cron restart
}

auto_check_process()
{
    if [ "$GPU_SERVER_RESTART" == "yes" ]; then
       echo "check_process has been created"
    else

        echo "*/1 * * * *  sh /autoshell/check_process.sh" >> /autoshell/scan.cron
        echo "*/1 * * * *  chown -R www-data:www-data /data/nextcloud/" >> /autoshell/scan.cron
    fi


}

main_loop()
{
    # step 1
    parse_arg

    # update .basrc
    if [ "$GPU_SERVER_RESTART" != "yes" ]; then

        # update .basrc
        append_to_bashrc "IPFS_PATH"  "export IPFS_PATH=/dbc/.ipfs"
        append_to_bashrc "CUDA_HOME"  "export CUDA_HOME=/usr/local/cuda"
        append_to_bashrc "LD_LIBRARY_PATH" "export LD_LIBRARY_PATH=/usr/local/nvidia/lib:/usr/local/nvidia/lib64:/usr/local/cuda/lib:/usr/local/cuda/lib64"
        update_path "/opt/conda/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/opt/anaconda3/bin:/usr/local/nvidia/bin:/dbc/code/bin:/root/anaconda3/bin:/usr/local/cuda/bin"


        source ~/.bashrc
    fi

    auto_scan_nextcloud

    setup_ssh_service
    start_nextcloud
    run_jupyter

    sleep 5s
    auto_check_process

    set_passwd

    if [ "$GPU_SERVER_RESTART" == "yes" ]; then
            echo " crontab /autoshell/scan.cron has been created"
    else
            crontab /autoshell/scan.cron
    fi

    if [ "$GPU_SERVER_RESTART" == "yes" ]; then
        echo "GPU_SERVER_RESTART "
    else

        export GPU_SERVER_RESTART="yes"

    fi

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













