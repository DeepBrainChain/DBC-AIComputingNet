#!/bin/bash
#set -x

#./dbc_task.sh Qme2UKa6yi9obw6MUcCRbpZBUmqMnGnznti4Rnzba5BQE3 QmbA8ThUawkUNtoV7yjso6V8B1TYeCgpXDhMAfYCekTNkr hello_world.py


if [ $# -ne 3 ]; then
	echo "params count is 3, please check params error"
	exit
fi

#define variable
exit_code=0
sleep_time=5s
home_dir=/dbc
data_dir_hash=$1
code_dir_hash=$2
task=$3


echo "======================================================="
echo "begin to exec dbc_task.sh"

#init ipfs
echo "begin to init ipfs"
ipfs init &

exit_code=$?
#echo -n "init ipfs exitcode:"
#echo $exit_code

if [ $exit_code -ne 0 ]; then
	echo "init ipfs failed and dbc_task.sh exit"
	exit
fi

echo "end to init ipfs"
sleep $sleep_time



#start ipfs
echo "======================================================="
echo "begin to start ipfs"
ipfs daemon &

exit_code=$?
#echo -n "start ipfs exitcode:"
#echo $exit_code

if [ $exit_code -ne 0 ]; then
	echo "start ipfs failed and dbc_task.sh exit"
	exit
fi

echo "end to start ipfs"
sleep $sleep_time


#create dir
echo "======================================================="
if [ ! -d $home_dir ]; then
	mkdir $home_dir
fi

cd $home_dir

#download data_dir
echo "======================================================="
echo -n "begin to download data dir:"
echo $data_dir_hash
ipfs get $data_dir_hash

exit_code=$?
#echo -n "download data dir exitcode:"
#echo $exit_code

if [ $? -ne 0 ]; then
	echo "download data dir failed and dbc_task.sh exit"
	exit
fi

echo -n "end to download data dir:"
echo $data_dir_hash
sleep $sleep_time



#download code_dir
echo "======================================================="
echo -n "begin to download code dir"
echo $code_dir_hash
ipfs get $code_dir_hash

exit_code=$?
#echo -n "download data dir exitcode:"
#echo $exit_code

if [ $? -ne 0 ]; then
	echo "download code dir failed and dbc_task.sh exit"
	exit
fi

echo -n "end to download code dir"
echo $code_dir_hash
sleep $sleep_time

#start exec task
echo "======================================================="
echo -n "begin to exec task"
echo $home_dir/$code_dir_hash/$task
python $home_dir/$code_dir_hash/$task

if [ $? -ne 0 ]; then
	echo "exec task failed and dbc_task.sh exit"
	exit
fi

echo -n "end to exec task"
echo $home_dir/$code_dir_hash/$task


#stop ipfs
PROC_NAME='ipfs'
PROC_ID=`ps -aef | grep ipfs | grep -v grep | awk ' {print $2}'`
echo -n "ipfs daemon pid:"
echo $PROC_ID
kill -TERM $PROC_ID

echo "======================================================="
echo "end to exec dbc_task.sh and ready to say goodbye!:-)"



