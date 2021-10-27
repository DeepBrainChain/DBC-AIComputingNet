#!/bin/bash

workdir=$(cd $(dirname $0) && pwd)
cd $workdir
if [ ! -d $workdir/log ]; then
    mkdir -p $workdir/log
fi

runlog=$workdir/log/detail.log
runlogbak=$workdir/log/bakdetail.log
if [ ! -e $runlog ];then
    >$runlog
fi
runlogsize=$(ls -lk $runlog|awk '{print $5}')
if [ $runlogsize -gt 100000 ]; then
    mv $runlog $runlogbak
fi

time=$(date "+%Y-%m-%d-%H:%M:%S")

. ./dbc_process.conf

check_process() {
    whami=$(whoami)
    i=0
    while [ $i -le $process_list_num ]
    do
        trytimes=0
        while [ $trytimes -lt 5 ]
        do
            trytimes=`expr $trytimes + 1`
            # group中所有进程名，机器数量的信息
            process_path=`echo ${process[$i]}|awk -F',' '{print $1}'`
            process_full_name=$process_path

            process_num=`ps aux|grep "$process_full_name"|grep ${whami}|grep -v "grep"|grep -v "gdb"|grep -vE "vim|rm|scp"|wc -l`
            if [ $process_num -eq 0 ]; then
                echo "$time 进程 $process_full_name 不存在" >> $runlog
                cd $process_path/shell
                echo "$time 调用stop脚本，stop脚本输出如下" >> $runlog
                /bin/bash ./stop.sh >> $runlog 2>&1
                sleep 1
                echo "$time 调用start脚本，start产生结果如下" >> $runlog
                /bin/bash ./start.sh >> $runlog 2>&1
                sleep 1
                cd $workdir
            else
                echo "check result:$time 进程 $process_full_name 工作正常" >> $runlog
                break
            fi
        done

        if [ $trytimes -ge 5 ]; then
            echo "$time 进程 $process_full_name 重试 $trytimes 仍然失败" >> $runlog
        elif [ $trytimes -eq 1 ]; then
            echo "$time 进程 $process_full_name 检测正常，第 $trytimes 次检查" >> $runlog
        else
            echo "$time 第 `expr $trytimes  - 1` 次，进程 $process_full_name 拉起成功" >> $runlog
        fi

        i=`expr $i + 1`
    done
}

shellname=`basename $0`
if fuser $0 2>/dev/null |sed "s/\\<$$\\>//" |grep -q '[0-9]';then
    echo "$time 拉起脚本 $shellname 已存在，进程信息如下" >> $runlog
    ps -ef|grep "$shellname$"|grep -v 'grep' >> $runlog
    exit 1
fi

cd $workdir
cd - >/dev/null 2>&1

# 获取进程数目
process_list_num=`echo ${!process[@]} |awk '{print $NF}'`

check_process
