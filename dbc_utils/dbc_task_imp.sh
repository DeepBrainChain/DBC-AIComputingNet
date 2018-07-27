#!/bin/bash
#set -x

#./dbc_task.sh Qme2UKa6yi9obw6MUcCRbpZBUmqMnGnznti4Rnzba5BQE3 QmbA8ThUawkUNtoV7yjso6V8B1TYeCgpXDhMAfYCekTNkr hello_world.py


if [ $# -lt 3 ]; then
        echo "params count less than 3, please check params"
        echo "params: $@"
        exit
fi


SYSTEM=`uname -s`

#define variable
exit_code=0
sleep_time=5s
wait_ipfs_init_time=10s
wait_ipfs_daemon_time=30s
home_dir=/dbc
data_dir_hash=$1
code_dir_hash=$2

task=$3
opts="${@:2}"


echo "task: " $task

myecho()
{
    if [ "$SYSTEM" = "Linux" ]; then
        echo -e $1
        return
    fi

    echo $1
}


upload_result_file()
{
    echo "start to upload training_result"
    if [ ! -f "/$home_dir/output/training_result_file" ]; then
        echo "can not find training result file"
        exit
    fi

    upload.sh /$home_dir/output

    echo " please check the training result, e.g."
    echo "     ipfs cat /ipfs/[DIR_HASH]/training_result_file"
}

stop_ipfs()
{
    #stop ipfs
    echo "======================================================="
    PROC_NAME='ipfs'
    PROC_ID=`ps -aef | grep ipfs | grep -v grep | awk ' {print $2}'`
    echo -n "ipfs daemon pid: "
    echo $PROC_ID
    kill -TERM $PROC_ID

    cd /
    rm -rf $home_dir/*

    echo "======================================================="
    echo "end to exec dbc_task.sh and ready to say goodbye! :-)"
}

end_ai_training()
{
    echo -n "end to exec task: "
    echo $home_dir/code/$task

    upload_result_file
    myecho "\n\n"
    stop_ipfs
}

echo "======================================================="
echo "begin to exec dbc_task_imp.sh"

#init ipfs
echo "begin to init ipfs"
ipfs init &
sleep $wait_ipfs_init_time

exit_code=$?
#echo -n "init ipfs exitcode:"
#echo $exit_code

if [ $exit_code -ne 0 ]; then
        echo "init ipfs failed and dbc_task.sh exit"
        exit
fi

echo "end to init ipfs"
sleep $sleep_time

cp /swarm.key /root/.ipfs/

#start ipfs
echo "======================================================="
echo "begin to start ipfs"
ipfs daemon &
sleep $wait_ipfs_daemon_time

exit_code=$?
#echo -n "start ipfs exitcode:"
#echo $exit_code

if [ $exit_code -ne 0 ]; then
        echo "start ipfs failed and dbc_task.sh exit"
        exit
fi

#add ipfs bootstrap node
ipfs bootstrap rm --all

#ipfs bootstrap add /ip4/114.116.19.45/tcp/4001/ipfs/QmPEDDvtGBzLWWrx2qpUfetFEFpaZFMCH9jgws5FwS8n1H
ipfs bootstrap add /ip4/49.51.49.192/tcp/4001/ipfs/QmRVgowTGwm2FYhAciCgA5AHqFLWG4AvkFxv9bQcVB7m8c
ipfs bootstrap add /ip4/49.51.49.145/tcp/4001/ipfs/QmPgyhBk3s4aC4648aCXXGigxqyR5zKnzXtteSkx8HT6K3
ipfs bootstrap add /ip4/122.112.243.44/tcp/4001/ipfs/QmPC1D9HWpyP7e9bEYJYbRov3q2LJ35fy5QnH19nb52kd5


echo "ipfs start complete"
sleep $sleep_time

#create dir
echo "======================================================="
if [ ! -d $home_dir ]; then
        mkdir $home_dir
fi

rm -rf $home_dir/*
mkdir $home_dir/output

echo -n "cd "
echo $home_dir
cd $home_dir

#download code_dir
echo "======================================================="
if [ -z "$code_dir_hash" ]; then
          echo -n "code_dir is empty.pls check"
          exit
fi
echo -n "begin to download code dir: "
echo $code_dir_hash
ipfs get $code_dir_hash -o ./code

exit_code=$?
#echo -n "download data dir exitcode: "
#echo $exit_code

if [ $? -ne 0 ]; then
        echo "download code dir failed and dbc_task.sh exit"
        exit
fi

if [ ! -e "$home_dir/code/$task" ]; then
   echo "task not exist. quit now"
   rm -rf $home_dir/code
   stop_ipfs
   exit
fi

echo -n "end to download code dir: "
echo $code_dir_hash

#download data_dir
echo "======================================================="
if [ -n "$data_dir_hash" ]; then
    echo -n "begin to download data dir: "
    echo $data_dir_hash
    ipfs get $data_dir_hash -o ./data

    exit_code=$?
    #echo -n "download data dir exitcode: "
    #echo $exit_code

    if [ $? -ne 0 ]; then
        echo "download data dir failed and dbc_task.sh exit"
        stop_ipfs
        exit
    fi

    echo -n "end to download data dir: "
    echo $data_dir_hash

    # add link from code_dir to data dir
    cp -rs $home_dir/data/* $home_dir/code/ > /dev/null 2>&1

    sleep $sleep_time
fi

myecho "\n\n"

sleep $sleep_time

#start exec task
echo "======================================================="
echo -n "begin to exec task: "
echo "$home_dir/code/$task"
#python $home_dir/$code_dir_hash/$task

cd $home_dir/code
dos2unix $task
chmod +x  $task
/bin/bash $task $opts | tee $home_dir/output/training_result_file

if [ $? -ne 0 ]; then
    echo "exec task failed and dbc_task.sh exit"
    end_ai_training
    exit
fi


end_ai_training
            
