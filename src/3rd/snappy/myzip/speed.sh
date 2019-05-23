#!/usr/bin/env bash


get_time_in_ms()
{
    os_name=`uname -a | awk '{print $1}'`
    if [ $os_name == 'Darwin' ];then
        gdate +%s%N
    else
        date +%s%N
    fi
}

ts=$(get_time_in_ms)


# validate input cmd
if [ $# -lt 1 ]; then
    echo "$0 [cmd] [argument ...]"
    exit 1
fi

# run input cmd
$@

tt=$((  ($(get_time_in_ms) - $ts)/1000000 ))
echo "Time taken: $tt milliseconds"