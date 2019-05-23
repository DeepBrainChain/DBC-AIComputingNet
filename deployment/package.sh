#!/bin/bash


base_dir=..

os_name=`uname -a | awk '{print $1}'`
if [ $os_name == 'Linux' ];then
    os_name=linux
elif [ $os_name == 'Darwin' ];then
    os_name=macos
fi

# get default version from src file: version.h

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
    version_file=$base_dir/src/core/common/version.h

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

version=$1

if [ -z "$version" ]; then
    version=$(get_version)
    echo "verson: $version"
fi


if [ "$os_name" == "macos" ]; then
    bash ./package_imp.sh $version "client"
fi


if [ "$os_name" == "linux" ]; then
    bash ./package_imp.sh $version "client"
    bash ./package_imp.sh $version "mining"
fi






