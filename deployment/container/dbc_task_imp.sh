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
home_dir=/dbc

# the first argument is either data_dir_hash or restart flag
data_dir_hash=$1
restart=$1


code_dir_hash=$2
ipfs_get_task_pid=12345

task=$3
opts="${@:4}"


start_ipfs_daemon()
{
    USERID=`id -u`
    PROC_NAME="ipfs"
    ps_="ps -e -o uid -o pid -o command"
    ipfs_pid=$($ps_  | grep [d]aemon | awk '{if (($1 == "'${USERID}'") && ($3~/'${PROC_NAME}'$/)) print $2}')
    if [ -z "$ipfs_pid" ]; then
        echo "ipfs daemon is starting"
        nohup ipfs daemon --enable-gc >/dev/null 2>&1 &
    else
        # ipfs is running
        return 0
    fi

    # wait process ready
    sleep 1
    ipfs_pid=$($ps_  | grep [d]aemon | awk '{if (($1 == "'${USERID}'") && ($3~/'${PROC_NAME}'$/)) print $2}')
    if [ -z "$ipfs_pid" ]; then
        echo "error: fail to start ipfs daemon"
        return 1
    else
        # loop until ipfs daemon is ready for use
        for i in {1..60}; do
            echo -n "."
            if ipfs swarm peers &>/dev/null; then
                echo
                echo "ipfs is started"
                return 0
            fi
            sleep 1
        done
    fi

    echo
    echo "error: fail to start ipfs daemon!"
    return 1
}

setup_ipfs()
{
    ipfs_tgz=go-ipfs_v0.4.15_linux-amd64.tar.gz
    ipfs_install_path=/dbc/.ipfs

    export IPFS_PATH=$ipfs_install_path

    if [ "$restart" != "true" ]; then
        cp -f /home/dbc_utils/$ipfs_tgz /tmp/
        cp -f /home/dbc_utils/swarm.key /tmp/
        cp -f /home/dbc_utils/install_ipfs.sh /tmp/
        # install ipfs repo

        pwd_now=$(pwd)
        cd /tmp
        mkdir $ipfs_install_path
        bash ./install_ipfs.sh ./$ipfs_tgz
        cd $pwd_now

        #set ipfs repo
        ipfs config Datastore.StorageMax 100GB

    else
        if ! start_ipfs_daemon; then
                echo "task completed: $task_id"
                exit 1
        fi
        cd /
    fi
}

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

    dbc_upload /$home_dir/output raw

    echo " please check the training result, e.g."
    echo "     ipfs cat /ipfs/[DIR_HASH]/training_result_file"
    echo " or download the training result, e.g."
    echo "     dbc>>> result -t [task_id] -o [dir]"
}

stop_ipfs()
{
    #stop ipfs
    echo "======================================================="
    USERID=`id -u`
    PROC_NAME="ipfs"
    ps_="ps -e -o uid -o pid -o command"

    pid=$($ps_  | grep [d]aemon |  awk '{if (($1 == "'${USERID}'") && ($3~/'${PROC_NAME}'$/)) print $2}')

    if [[ -n "$pid" ]]; then
        echo "ipfs daemon pid: $pid"
        echo "stop ipfs daemon"
        ipfs shutdown > /dev/null 2>&1
        kill -TERM $pid > /dev/null 2>&1
    fi

    cd /
    # keep user data and let external data management strategy to handle it
#    rm -rf $home_dir/*

    echo "======================================================="
    echo "end to exec dbc_task.sh and ready to say goodbye! :-)"
}

end_ai_training()
{
    echo -n "end to exec task: "
    echo $home_dir/code/$task

#    upload_result_file

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
    max_retry=10
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

    # extract tar file
    f_array=$(ls $2/*.ipfs.upload.tar 2>/dev/null)
    for i in ${f_array[@]}; do
        echo "extract tar file $i"
        tar xf $i -C $2 > /dev/null 2>&1

        if [ $? -ne 0 ]; then
            echo "fail to extract $i "
            rm -f $i

            return 1
        fi

        rm -f $i
    done

    return 0
}

echo "======================================================="
echo "begin to exec dbc_task_imp.sh"

#create dir
echo "======================================================="
if [ ! -d $home_dir ]; then
        mkdir $home_dir
fi

if [ "$restart" != "true" ]; then
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


    if [ -d /home/dbc_utils/cache/$code_dir_hash ]; then
        echo "cache hit"
        cp -r /home/dbc_utils/cache/$code_dir_hash ./code
    else
        #
        setup_ipfs

        download $code_dir_hash ./code
        #ipfs get $code_dir_hash -o ./code

        if [ $? -ne 0 ]; then
                echo "download code dir failed and dbc_task.sh exit"
                stop_ipfs
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
    fi


    #download data_dir
#    echo "======================================================="
#    if [ -n "$data_dir_hash" ]; then
#        echo -n "begin to download data dir: "
#        echo $data_dir_hash
#
#    #    ipfs get $data_dir_hash -o ./data
#        download $data_dir_hash ./data
#
#        if [ $? -ne 0 ]; then
#            echo "download data dir failed and dbc_task.sh exit"
#            stop_ipfs
#            exit
#        fi
#
#        echo -n "end to download data dir: "
#        echo $data_dir_hash
#
#        sleep $sleep_time
#    fi

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
/bin/bash $task $opts 2>&1 | tee $home_dir/output/training_result_file

if [ $? -ne 0 ]; then
    echo "exec task failed and dbc_task.sh exit"
    end_ai_training
    exit
fi

end_ai_training