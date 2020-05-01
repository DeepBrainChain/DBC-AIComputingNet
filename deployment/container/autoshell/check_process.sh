#! /bin/bash


sshd_number=`ps -ef |grep -w sshd|grep -v grep|wc -l`
if [ $sshd_number -le 0 ];then
  sudo  service ssh restart
fi

jupyter_number=`ps -ef |grep -w jupyter-lab|grep -v grep|wc -l`
if [ $jupyter_number -le 0 ];then
   sudo nohup jupyter-lab --ip 0.0.0.0 --port 8888 --no-browser --allow-root &
fi


mysql_number=`ps -ef |grep -w mysqld_safe|grep -v grep|wc -l`
if [ $mysql_number -le 0 ];then
  sudo service mysql restart
fi

mysqld_number=`ps -ef |grep -w mysqld|grep -v grep|wc -l`
if [ $mysqld_number -le 0 ];then
  sudo service mysql restart
fi


redis_number=`ps -ef |grep -w redis-server|grep -v grep|wc -l`
if [ $redis_number -le 0 ];then
   sudo redis-server /etc/redis/redis.conf
fi


apache2_number=`ps -ef |grep -w apache2|grep -v grep|wc -l`
if [ $apache2_number -le 0 ];then
   sudo  service apache2 restart
fi


exit
