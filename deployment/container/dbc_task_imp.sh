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
ipfs_get_task_pid=12345

task=$3
opts="${@:4}"

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

progress()
{
    dir=$1
    cmd="du -sk $dir | awk '{print $1}'"
    a=0
    t=10
    count=0

    while true; do
        n=$(ps -ef | grep [i]pfs | grep get | wc -l)

        b=$(du -sk $dir | awk '{print $1}')
        bm=$(( b/1024 ))
        d=$(( b-a ))
        speed=$(( d/t ))

        if [ $bm -eq 0 ]; then
            echo "recv: $(( b )) KB, speed: $speed KB/s"
        else
            echo "recv: $(( bm )) MB, speed: $speed KB/s"
        fi

        if [ $a -eq $b ]; then
            count=$(( count+1 ))
            if [ $count -eq 6 ]; then
                echo "download operation stucked"
                kill -9 $ipfs_get_task_pid
                return 1
            fi
        else
            count=0
        fi

        a=$b

        if [ $n -eq 0 ]; then
            echo "ipfs get end"
            return 0
        else
            sleep $t
        fi
    done

}


download_run_once()
{
    max_wait_time=60
    hash=$1
    dir=$2

    rm -rf ./$dir
    echo -n "search "

    ipfs get $hash -o $dir >/dev/null 2>&1  &
    ipfs_get_task_pid=$!
#    echo "ipfs get task's pid: $ipfs_get_task_pid"


    found=1
    for i in `seq 1 $max_wait_time`;
    do
        if [ -e $dir ]; then
            found=0
            echo
            break
        else
        echo -n "."
        fi
        sleep 1
    done

    if [ $found -eq 1 ]; then
        echo
        echo "fail to find $hash in ipfs storage"
        kill -9 $ipfs_get_task_pid >/dev/null 2>&1
        return 1
    else
        progress $2
        if [ $? -ne 0 ]; then
            return 1
        fi
#        wait
        return 0
    fi

}

download()
{
    max_retry=5
    ok=1

    for i in `seq 1 $max_retry`;
    do
        download_run_once $1 $2
        if [ $? -eq 0 ]; then
            ok=0
            break
        fi
        echo "retry ..."
    done

    if [ $ok -eq 1 ]; then
        echo "error | fail to download "
        return 1
    fi

    return 0
}



echo "======================================================="
echo "begin to exec dbc_task_imp.sh"


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

download $code_dir_hash ./code
#ipfs get $code_dir_hash -o ./code

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

#    ipfs get $data_dir_hash -o ./data
    download $data_dir_hash ./data

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
            
