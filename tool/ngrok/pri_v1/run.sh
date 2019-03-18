#!/bin/bash

# {"server": [{"ip": "116.85.24.172", "dn": "ngrok.dbc.com:20443"}], "token": "dbc_test", "services": ["ssh", "jupyter"], "id_pub": "....", "pwd": "12345678"}
#

args="$@"
echo $args

test -f ./jq && chmod +x ./jq

debug=($(echo $args | ./jq -r .debug?))

if [ "$debug" == "true" ]; then
    set -x
fi

# mandatory
server_ips=($(echo $args | ./jq -r .server[].ip?))
server_dns=($(echo $args | ./jq -r .server[].dn?))

token=$(echo $args | ./jq -r .token?)

if [ "$token"] == "null" ]; then
    echo "Error: token missing"
fi

# optional
services=$(echo $args | ./jq -r .services[]?)
id_pub=$(echo $args | ./jq -r .id_pub?)
default_pwd=$(echo $args | ./jq -r .pwd?)

if [ -z "$services" ]; then services=("ssh"); fi

rm -rf /tmp/bye

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

# update .basrc
append_to_bashrc "export IPFS_PATH=/dbc/.ipfs"
append_to_bashrc "export LD_LIBRARY_PATH=/usr/local/nvidia/lib:/usr/local/nvidia/lib64"
#append_to_bashrc "export NVIDIA_VISIBLE_DEVICES=all"
#append_to_bashrc "export NVIDIA_DRIVER_CAPABILITIES=compute,utility"
update_path "/opt/conda/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin"
update_path "/usr/local/nvidia/bin"

if ! which sshd
then
    apt update >/dev/null
    apt install openssh-server vim screen -y >/dev/null
fi

TARGET_KEY="TCPKeepAlive"
REPLACEMENT_VALUE="no"
sed -i "s/\($TARGET_KEY * *\).*/\1$REPLACEMENT_VALUE/" /etc/ssh/sshd_config

# set initial password for root
if [ "$default_pwd" != "null" ]; then
    sed  -i "s/prohibit-password/yes/" /etc/ssh/sshd_config
    echo root:$default_pwd|chpasswd
fi

/etc/init.d/ssh restart


# allow ssh through ssh-rsa key
mkdir -p ~/.ssh
if [ "$id_pub" != "null" ]; then
    echo ssh-rsa $id_pub >> ~/.ssh/authorized_keys
fi

#sleep 5s



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


# ngrok v2
cd `dirname $0`/ng_v1
test -f ./ngrok && chmod +x ./ngrok


# for each server, for each port, do port proxy

for i in "${!server_ips[@]}"
do
    server_ip=${server_ips[$i]}
    server_dn=${server_dns[$i]}
    # connect to dbc ngrok server version 2
    #echo "auth_token: $token" >> ./ssh.yml
    #ip_addr="114.115.219.202"

    dn=$(echo $server_dn | cut -d ":" -f 1)
    host_ip="$server_ip   $dn"
    if ! grep "$host_ip" /etc/hosts; then
        echo "$host_ip" >> /etc/hosts
    fi

    port_http=4050
    for service in $services
    do

        case $service in
            ssh)
                # startapp <ngrok> <yml> <target> <token>
                sed -i "s#^.*server_addr:.*#server_addr: ${server_dn}#g" ./${service}.yml
                sed -i "s#^.*inspect_addr:.*#inspect_addr: ${port_http}#g" ./${service}.yml
                screen -d -m bash ./startapp ./${service}.yml $service $token

                if print_tcp_port $port_http; then
                    echo "ssh -p $port root@${server_ip}"
                    port_=$(($port_http+1))
                fi

            ;;

            jupyter)
                sed -i "s#^.*server_addr:.*#server_addr: ${server_dn}#g" ./${service}.yml
                sed -i "s#^.*inspect_addr:.*#inspect_addr: ${port_http}#g" ./jupyter.yml
                screen -d -m bash ./startapp ./${service}.yml $service $token

                if print_tcp_port $port_http; then
                    echo "jupyter url:  http://${server_ip}:${port}"
                    port_=$(($port_http+1))
                fi
            ;;

            *)
                echo "unknown service $service"
            ;;
        esac
    done

done

if [ "$debug" == "true" ]; then
    set +x
fi

# loop
while ! ls /tmp/bye &>/dev/null; do
   sleep 10s
done

echo "bye"
rm -f /tmp/bye

