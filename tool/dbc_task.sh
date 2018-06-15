#!/bin/bash
#set -x

#./dbc_task.sh Qme2UKa6yi9obw6MUcCRbpZBUmqMnGnznti4Rnzba5BQE3 QmbA8ThUawkUNtoV7yjso6V8B1TYeCgpXDhMAfYCekTNkr hello_world.py


if [ $# -ne 3 ]; then
	echo "params count is 3, please check params error"
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


myecho()
{
    if [ "$SYSTEM" = "Linux" ]; then
        echo -e $1
        return
    fi
    
    echo $1
}



myecho "\n\n"

echo "======================================================="
echo "begin to exec dbc_task.sh"

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

myecho "\n\n"

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

ipfs bootstrap add /ip4/114.116.19.45/tcp/4001/ipfs/QmPEDDvtGBzLWWrx2qpUfetFEFpaZFMCH9jgws5FwS8n1H


echo "end to start ipfs"
sleep $sleep_time



myecho "\n\n"

#create dir
echo "======================================================="
if [ ! -d $home_dir ]; then
	mkdir $home_dir
fi

echo -n "cd "
echo $home_dir
cd $home_dir


myecho "\n\n"

#download data_dir
echo "======================================================="
echo -n "begin to download data dir: "
echo $data_dir_hash
ipfs get $data_dir_hash

exit_code=$?
#echo -n "download data dir exitcode: "
#echo $exit_code

if [ $? -ne 0 ]; then
	echo "download data dir failed and dbc_task.sh exit"
	exit
fi

echo -n "end to download data dir: "
echo $data_dir_hash
sleep $sleep_time



myecho "\n\n"

#download code_dir
echo "======================================================="
echo -n "begin to download code dir: "
echo $code_dir_hash
ipfs get $code_dir_hash

exit_code=$?
#echo -n "download data dir exitcode: "
#echo $exit_code

if [ $? -ne 0 ]; then
	echo "download code dir failed and dbc_task.sh exit"
	exit
fi

echo -n "end to download code dir: "
echo $code_dir_hash
sleep $sleep_time


myecho "\n\n"

#start exec task
echo "======================================================="
echo -n "begin to exec task: "
echo $home_dir/$code_dir_hash/$task
cd $home_dir/$code_dir_hash/
python ./$task | tee /training_result_file

if [ $? -ne 0 ]; then
	echo "exec task failed and dbc_task.sh exit"
	exit
fi

echo -n "end to exec task: "
echo $home_dir/$code_dir_hash/$task

echo "start to upload training_result"
python /upload_training_result.py



myecho "\n\n"


#stop ipfs
echo "======================================================="
PROC_NAME='ipfs'
PROC_ID=`ps -aef | grep ipfs | grep -v grep | awk ' {print $2}'`
echo -n "ipfs daemon pid: "
echo $PROC_ID
kill -TERM $PROC_ID


myecho "\n\n"

echo "======================================================="
echo "end to exec dbc_task.sh and ready to say goodbye! :-)"



