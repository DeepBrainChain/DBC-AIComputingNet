#!/bin/bash

echo "==================================="
echo "package start: $version $type"

base_dir=../..
tool_dir=$base_dir/tool
shell_dir=$base_dir/shell
deployment_dir=$base_dir/deployment

os_name=`uname -a | awk '{print $1}'`
if [ $os_name == 'Linux' ];then
    os_name=linux
elif [ $os_name == 'Darwin' ];then
    os_name=macos
fi

remove_leading_zero()
{
    v=$1
    v=$(echo "$v" |  sed 's/^0*//')

    if [ -z "$v" ]; then
        v="0"
    fi
    echo $v
}

get_version()
{
    version_file=$base_dir/src/dbc/core/common/version.h

    ver=$(grep "#define CORE_VERSION" $version_file  | awk '{print $3}')

    v1="${ver:2:2}"
    v1=$(remove_leading_zero $v1)

    v2="${ver:4:2}"
    v2=$(remove_leading_zero $v2)

    v3="${ver:6:2}"
    v3=$(remove_leading_zero $v3)

    v4="${ver:8:2}"
    v4=$(remove_leading_zero $v4)

    echo "$v1.$v2.$v3.$v4"
}

help()
{
    echo "synopsis: bash ./package.sh <version> <type>"
    echo "type: client or mining"
    echo "pls input version and type, e.g. bash ./package.sh 0.3.0.1 client"
    exit
}

# dbc install pkg: dbc_repo.tar.gz
dbc_package()
{
    echo "step: dbc program package"

    # mkdir $dbc_repo_dir
    mkdir $dbc_repo_dir/shell

    # dbc exe and default configure
    cp $base_dir/output/dbc $dbc_repo_dir/

    # strip
    if [ $os_name == 'linux' ]; then
        strip --strip-all $dbc_repo_dir/dbc
    fi

    if [ $os_name == 'macos' ]; then
        strip -S $dbc_repo_dir/dbc
    fi

    cp -r $base_dir/conf $dbc_repo_dir/

    if [ $type == 'client' ];then
        rm -rf $dbc_repo_dir/conf/container.conf
        cp $shell_dir/client/*.sh $dbc_repo_dir/shell/
        cp $shell_dir/update_client.sh $dbc_repo_dir/update.sh
        chmod +x $dbc_repo_dir/update.sh
    fi

    if [ $type == 'mining' ]; then
        cp $shell_dir/mining/*.sh $dbc_repo_dir/shell/
        cp $shell_dir/update_mining.sh $dbc_repo_dir/update.sh
        chmod +x $dbc_repo_dir/update.sh
    fi

    cp -r $shell_dir/crontab $dbc_repo_dir/shell

    cp $tool_dir/add_dbc_user.sh    $dbc_repo_dir/shell/
    cp $tool_dir/change_gpu_id.sh   $dbc_repo_dir/shell/
    cp $tool_dir/clean_cache.sh     $dbc_repo_dir/shell/
    cp $tool_dir/dbc_upload         $dbc_repo_dir/shell/

    chmod +x $dbc_repo_dir/dbc
    chmod +x $dbc_repo_dir/shell/*.sh
    chmod +x $dbc_repo_dir/shell/crontab/*.sh
}

if [ $# -lt 2 ]; then
    echo "need more arguments"
    help
    exit 1
fi

version=$1
type=$2

if [ -z "$version" ]; then
    version=$(get_version)
    echo "verson: $version"
    exit 0
fi

if [ "$type" != "client" -a "$type" != "mining" ]; then
    echo "unknown type"
    help
fi

tar_ball=dbc_${os_name}_${type}_${version}.tar.gz
rm -f ./package/$tar_ball
rm -rf ./package/dbc_$type\_node
mkdir -p ./package/dbc_$type\_node
cd ./package

pwd

dbc_repo_dir=./dbc_$type\_node

dbc_package

# package
tar -czvf $tar_ball dbc_$type\_node

rm -rf $dbc_repo_dir

echo "package complete: $version $type"
