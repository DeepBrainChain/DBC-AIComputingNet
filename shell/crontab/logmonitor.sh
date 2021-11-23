#!/bin/bash

today=$(date +%F)
month_day=$(date -d '1 month ago' +%s)

compress_folder() {
  echo "compress $1"
  old_dir=`pwd`
  cd $1
  cur_dir=`pwd`
  rm -f $cur_dir.tar.gz
  tar -czvf $cur_dir.tar.gz *
  cd $old_dir
}

foreach_logs() {
  # delete log files older than one month
  fileList=`ls $1/matrix_core_* -1 -c`
  for fileName in $fileList
  do
    fileDate=$(stat -c %Y $fileName | awk '{print $1}')
    if [ "$fileDate" != "" ] && [[ $fileDate -lt $month_day ]];then
      # echo "older than one month $fileName"
      rm -f $fileName
    fi
  done
  # compress log files older than one day
  logList=`ls $1/matrix_core_*log -1 -c`
  for logName in $logList
  do
    # echo $logName
    fileDate=$(stat -c %y $logName | awk '{print $1}')
    if [[ $fileDate != $today ]];then
      if [ ! -d $1/matrix_core_$fileDate ];then
        mkdir -p $1/matrix_core_$fileDate
      fi
      mv $logName $1/matrix_core_$fileDate
    fi
  done
  folderList=`ls -1 -d $1/matrix_core_*/`
  for folder in $folderList
  do
    compress_folder $folder
    rm -rf $folder
  done
}

foreach_process() {
  i=0
  while [ $i -le $process_list_num ]
  do
    process_path=`echo ${process[$i]}|awk -F',' '{print $1}'`
    process_full_name=$process_path
    if [ -d $process_full_name/logs ];then
      foreach_logs $process_full_name/logs
    fi
    i=`expr $i + 1`
  done
}

. ./dbc_process.conf
process_list_num=`echo ${!process[@]} |awk '{print $NF}'`
# echo $process_list_num
foreach_process
