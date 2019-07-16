#!/bin/bash
#set -x


pre_check()
{
    echo "INFO: pre_check start"

    if [[ ! -d $old_dbc_install_path ]]; then
        echo "ERROR: $old_dbc_install_path does not exist"
        return 1
    fi

    if [[ -d $new_dbc_install_path ]]; then
        echo "ERROR: $new_dbc_install_path already exist"
        return 1
    fi

    if ! which nvidia-docker &>/dev/null; then
        echo "WARNING: nvidia-docker not found"
    else
        echo "INFO: $(nvidia-docker version| head -n1)"
    fi

    if ! which docker &>/dev/null; then
        echo "WARNING: Docker not found"
    else
        echo "INFO: Docker $(docker version | grep Version | head -n1 |xargs)"
    fi

    echo "INFO: GPU card"
    if ! nvidia-smi -L; then
        echo "WARNING: nvidia driver not found"
    fi


    echo "INFO: pre_check complete"

    if [[ "$is_dry_run" ==  "true" ]]; then
        exit 0
    fi

    return 0
}

download_dbc_tar()
{
    echo "INFO: begin to wget DBC release package $dbc_tarball"
    rm -rf $dbc_tarball

    #wget https://github.com/DeepBrainChain/deepbrainchain-release/releases/download/latest/dbc-linux-mining-$release_version.tar.gz
    wget http://116.85.24.172:20444/static/$dbc_tarball
    if [[ $? -ne 0 || ! -f $dbc_tarball ]]; then
        echo "ERROR: wget DBC release package failed"
        return 1
    fi
    echo "INFO: wget DBC release package complete"
    echo -e

    return 0
}

stop_dbc_service()
{
    echo "INFO: stop dbc service"
    systemctl stop dbc.service

    stopapp

    echo

    sleep 5s

    return 0
}

install_dbc()
{
    echo "INFO: install dbc to dir $new_dbc_install_path"
    rm -rf $new_dbc_install_path
    mkdir $new_dbc_install_path
    if ! tar -zxvf $dbc_tarball -C $(dirname $new_dbc_install_path) >/dev/null; then
        echo "ERROR: fail to extract $dbc_tarball"
        return 1
    fi

    return 0
}

merge_dbc_data()
{
    echo "INFO: merge user data/conf"
    cp -r $old_dbc_install_path/dbc_repo/conf $new_dbc_install_path/dbc_repo/
    cp -r $old_dbc_install_path/dbc_repo/dat $new_dbc_install_path/dbc_repo/
    cp -r $old_dbc_install_path/dbc_repo/db $new_dbc_install_path/dbc_repo/

    return 0
}


install_dependency()
{
    echo "INFO: install other tools"

    return 0
}


post_config()
{
    echo "INFO: add dbc executable path to ENV PATH"

    if ! cat ~/.bashrc | grep "DBC_PATH=" &>/dev/null; then
        echo "export DBC_PATH=${new_dbc_install_path}/dbc_repo" >> ~/.bashrc
        echo 'export PATH=$PATH:$DBC_PATH' >> ~/.bashrc
        echo 'export PATH=$PATH:$DBC_PATH/tool' >> ~/.bashrc

        if ! grep "source .bashrc" ~/.bash_profile; then
            echo "source .bashrc" >> ~/.bash_profile
        fi

    else
        sed -i "/DBC_PATH=/c export DBC_PATH=${new_dbc_install_path}/dbc_repo" ~/.bashrc
    fi
    echo -e

    echo "INFO: set dbc service"
    user_name=`whoami`
    sudo rm -f /lib/systemd/system/dbc.service
    sudo touch /lib/systemd/system/dbc.service
    sudo chmod 777 /lib/systemd/system/dbc.service
    echo "[Unit]" >> /lib/systemd/system/dbc.service
    echo "Description=dbc Daemon" >> /lib/systemd/system/dbc.service
    echo "Wants=network.target" >> /lib/systemd/system/dbc.service
    echo "[Service]" >> /lib/systemd/system/dbc.service
    echo "Type=forking" >> /lib/systemd/system/dbc.service
    echo "User=$user_name" >> /lib/systemd/system/dbc.service
    #echo "Group=$user_name" >> /lib/systemd/system/dbc.service
    echo "WorkingDirectory=${new_dbc_install_path}/dbc_repo" >> /lib/systemd/system/dbc.service
    echo "ExecStart=${new_dbc_install_path}/dbc_repo/dbc --ai --daemon -n \"$host\"" >> /lib/systemd/system/dbc.service
    echo "ExecStop=${new_dbc_install_path}/dbc_repo/stopapp" >> /lib/systemd/system/dbc.service
    echo "[Install]" >> /lib/systemd/system/dbc.service
    echo "WantedBy=multi-user.target" >> /lib/systemd/system/dbc.service
    sudo chmod 644 /lib/systemd/system/dbc.service

    sudo systemctl daemon-reload
    sudo systemctl enable dbc.service

    echo "INFO: start dbc service"
    sudo systemctl start dbc.service


    sleep 5s
    sudo systemctl status dbc.service

    echo -e

    echo "INFO: register dbc private docker register"
    bash $new_dbc_install_path/dbc_repo/tool/private_docker_repo/register_docker_repo.sh

    return 0
}

usage() { echo "Usage: $0 [-t new] [-p]" 1>&2; exit 1; }

while getopts "pt:" o; do
    case "${o}" in
        p)
			echo "INFO: dry run"
            is_dry_run=true
            ;;
        t)
            new_ver=${OPTARG}
			;;
        *)
            usage
            ;;
    esac
done
shift $((OPTIND-1))

if [[ -z "${new_ver}" ]]; then
    usage
    exit 0
fi


dbc_tarball=dbc-linux-mining-$new_ver.tar.gz

host=$(hostname)
install_path=""
work_dir=$(pwd)

if which dbc &>/dev/null ; then
    old_ver=$(which dbc| awk -F "/" '{print $(NF-2)}')
else
    echo "ERROR: no dbc found from bash path"
    return 1
fi

old_dbc_install_path=$(dirname $(dirname `which dbc`))
new_dbc_install_path="$(dirname $(dirname $(dirname `which dbc`)))/${new_ver}"

echo "** dbc upgrade from $old_ver to $new_ver"

func_list=(pre_check download_dbc_tar stop_dbc_service install_dbc merge_dbc_data install_dependency post_config)

i=1
for func in ${func_list[@]}
do
    echo
    echo "[step $i] `echo $func`"
    echo
    if ! $func; then
        echo "** dbc upgrade fail at `echo $func` after $SECONDS second! **"
        exit 1
    fi

    let "i+=1"
done


echo
date
echo "** dbc upgrade complete in $SECONDS second! **"
echo

exit 0
