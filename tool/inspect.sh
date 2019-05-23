#!/bin/bash

echo "******view disk resource usage******"
df -lh
echo -e "\n"

echo "******check the dbc process is running******"
SYSTEM=`uname -s`
USERID=`id -u`

myecho()
{
    if [ "$SYSTEM" = "Linux" ]; then
        echo -e $1
        return
    fi

    echo $1
}


echo "UID        PID     PPID C STIME TTY      TIME     CMD"

NAME_PROC="dbc"


for PROC in ${NAME_PROC}
do

    ps -aef | awk '{if ((($1 == "'${LOGNAME}'") || ($1 == "'${USERID}'")) && (($8 == "'${PROC}'") || ($8~/\/'${PROC}'$/))) print}'

done

myecho "\n"

echo "******check some port is start up******"
netstat -anp | grep 31107
if [ $? -ne 0 ]; then
    echo "******docker 31107 port is not start up******"
    exit
fi

netstat -anp | grep 3476
if [ $? -ne 0 ]; then
    echo "******nvidia docker 3476 port is not start up******"
    exit
fi

