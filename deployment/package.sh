#!/bin/bash

workdir=$(cd $(dirname $0) && pwd)
cd $workdir

base_dir=${workdir}/..

remove_leading_zero() {
    v=$1
    v=$(echo "$v" |  sed 's/^0*//')

    if [ -z "$v" ]; then
        v="0"
    fi

    echo $v
}

version_file=$base_dir/src/dbc/common/version.h
ver=$(grep "#define CORE_VERSION" $version_file  | awk '{print $3}')

v1="${ver:2:2}"
v1=$(remove_leading_zero $v1)

v2="${ver:4:2}"
v2=$(remove_leading_zero $v2)

v3="${ver:6:2}"
v3=$(remove_leading_zero $v3)

v4="${ver:8:2}"
v4=$(remove_leading_zero $v4)

version="$v1.$v2.$v3.$v4"

echo "version:" $version

mkdir -p ./package
rm -rf ./package/*

/bin/bash ./package_imp.sh $version "client"
/bin/bash ./package_imp.sh $version "seed"
/bin/bash ./package_imp.sh $version "mining"
/bin/bash ./package_imp.sh $version "baremetal"
