#!/bin/bash

args="$@"
echo $args
URL_TOKEN=""
DEFAULT_PWD=""
SSH_INFO=""
jupyter_url=""
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
    server_ips=($(echo $args | ./jq -r .ngrok_server[].ip?))
    server_dns=($(echo $args | ./jq -r .ngrok_server[].dn?))

    token=$(echo $args | ./jq -r .ngrok_token?)

    if [ "$token" == "null" ]; then
        echo "Error: token missing"
        exit 1
    fi

    services=($(echo $args | ./jq -r .services? | ./jq -r 'keys'[]))
#    serviceports=($(echo $args | ./jq .services[]?))
    if [ "$services" == "null" ] ; then
        echo "Error: service missing"
        exit 1
    fi

    serviceports=()
    for service in "${services[@]}"
    do
        v=$(echo $args | ./jq -r .services.$service?)
        if [ "$v" == "null" ]; then
            echo "Error: port of $service missing"
            exit 1
        fi
        serviceports+=($v)
    done



    # optional, ssh public key
    id_pub=$(echo $args | ./jq -r .id_pub?)
}


run_jupyter()
{
   # JUPYTER_TOKEN=$(< /dev/urandom tr -dc A-Za-z0-9 | head -c64; echo)
   # nohup jupyter-notebook --ip 0.0.0.0 --port 8888 --no-browser --allow-root --NotebookApp.token=$JUPYTER_TOKEN &
   # URL_TOKEN="?token="$JUPYTER_TOKEN

    if [ "$GPU_SERVER_RESTART" == "yes" ]; then
        echo "keep jupyter current password"
    else
        JUPYTER_PASSWD=$(< /dev/urandom tr -dc A-Za-z0-9 | head -c8; echo)
        echo "JUPYTER_PASSWD:"$JUPYTER_PASSWD
        export GPU_SERVER_RESTART="yes"
        expect /chjupyter.exp $JUPYTER_PASSWD
    fi
    nohup jupyter notebook --ip 0.0.0.0 --port 8888 --no-browser --allow-root > jupyter.log 2>&1 &
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
        DEFAULT_PWD=$(< /dev/urandom tr -dc A-Za-z0-9 | head -c8; echo)
        if [ "$DEFAULT_PWD" != "null" ]; then
            echo root:$DEFAULT_PWD|chpasswd

        else
            echo "fail to set passwword"
        fi
    fi
}



port=""
print_tcp_port()
{
    http_port=$1

    for i in {1..20}; do
        sleep 3s
#        ip=$(curl -s localhost:$http_port/http/in | grep "tcp://" | sed -r 's#^.*tcp://(.*)\\",.*$#\1#g' | awk -F ":" '{print $1}' | xargs -I {}  grep {} /etc/hosts | awk '{print $1}')
        port=$(curl -s localhost:$http_port/http/in | grep "tcp://" | sed -r 's#^.*tcp://(.*)\\",.*$#\1#g' | awk -F ":" '{print $2}')
        if [ -n "$port" ]; then
            return 0
        fi
    done

    if [ ! -n $port ]; then
        curl localhost:$http_port/status 2>&1| grep tcp
        echo "fail to open public port"
        return 1
    fi

    return 1
}

print_login_info()
{
    sleep 3s
    echo $SSH_INFO
    echo $jupyter_url
}
create_yml_file()
{
    ngrok_dn=$1
    fn="${2}.yml"
    service=$2
    service_port=$3
    inspect_addr=$4

    echo "server_addr: $ngrok_dn" >$fn
    echo "tunnels:" >> $fn
    echo "  $service:" >> $fn
    echo "    proto:" >> $fn
    echo "      tcp: $3" >> $fn
    echo >> $fn
    echo "inspect_addr: $inspect_addr" >> $fn
}


setup_ngrok_connection()
{
    test -f ./ngrok && chmod +x ./ngrok

    # for each server, for each port, do port proxy
    for i in "${!server_ips[@]}"
    do
        server_ip=${server_ips[$i]}
        server_dn=${server_dns[$i]}


        dn=$(echo $server_dn | cut -d ":" -f 1)
        host_ip="$server_ip   $dn"
        if ! grep "$host_ip" /etc/hosts; then
            echo "$host_ip" >> /etc/hosts
        fi

        port_http=4050

        for j in "${!services[@]}"
    #    for service in $services
        do
            service=${services[$j]}
            serviceport=${serviceports[$j]}

            echo "configure ngrok mapping for $service $serviceport"
            create_yml_file $server_dn $service $serviceport $port_http

            cat ${service}.yml

            screen -d -m bash ./startapp ./${service}.yml $service $token

            if print_tcp_port $port_http; then
                echo "map $service to ${server_ip}:${port}"

                port_http=$(($port_http+1))
            else
                echo "fail to export $service"
                 if [ "$GPU_SERVER_RESTART" != "yes" ]; then
                    exit 1
                 fi

            fi
            run_jupyter

            # usage hint for ssh and jupyter
            case $service in
                ssh)
                    SSH_INFO="ssh_login_info: ssh -p ${port} root@${server_ip}; pwd:"${DEFAULT_PWD}
                ;;
                jupyter)
                    jupyter_url="jupyter url:  http://${server_ip}:${port}  "
                ;;
                jupyter_lab)
                    jupyter_lab_url="jupyter_lab_url:  http://${server_ip}:${port}  "
                ;;
                nextcloud)
                    nextcloud_url="nextcloud_url:  http://${server_ip}:${port}  "
                ;;
                *)
                ;;
            esac
        done

    done
}


main_loop()
{
    # step 1
    parse_arg

    # update .basrc
    append_to_bashrc "export IPFS_PATH=/dbc/.ipfs"
    append_to_bashrc "export LD_LIBRARY_PATH=/usr/local/nvidia/lib:/usr/local/nvidia/lib64"
    update_path "/opt/conda/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/opt/anaconda3/bin"
    update_path "/usr/local/nvidia/bin"


    source ~/.bashrc

    setup_ssh_service

    set_passwd



    # ngrok
    cd `dirname $0`/bin
    setup_ngrok_connection


    echo "gpu server is ready"

    rm -rf /tmp/bye

    if [ "$debug" == "true" ]; then
        set +x
    fi

    print_login_info

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
