#!/bin/bash

version=$1
type=$2

echo "==================================="
echo "package start: $version $type"

workdir=$(cd $(dirname $0) && pwd)
cd $workdir

base_dir=${workdir}/..

tool_dir=$base_dir/tool
shell_dir=$base_dir/shell

help() {
    echo "synopsis: bash ./package.sh"
    exit
}

dbc_package()
{
    echo "step: dbc program package"

    cp $base_dir/output/dbc $dbc_repo_dir/
    strip --strip-all $dbc_repo_dir/dbc

    cp -rf $base_dir/conf $dbc_repo_dir/

    mkdir -p $dbc_repo_dir/shell
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

tar_ball=./package/dbc_${type}_node_${version}.tar.gz
dbc_repo_dir=./dbc_${type}_node
mkdir -p ${dbc_repo_dir}

dbc_package

tar -czvf $tar_ball dbc_$type\_node

rm -rf $dbc_repo_dir

echo "package complete: $version $type"
