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
  for  fline  in  `cat ./dbc_dir.conf`; do
    process_path=$fline

    if [ -d $process_path/logs ];then
      foreach_logs $process_path/logs
    fi
  done
}

foreach_process
