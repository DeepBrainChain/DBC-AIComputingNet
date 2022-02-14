#!/bin/sh

WORKDIR=$( cd $( dirname $0 ) && pwd )

if [ $# -lt 6 ]
then
	echo "Usage:`basename $0` host  ssh_port  username  passwd  remote_file  local_dir"
	exit 1
fi

HOST=$1
PORT=$2
USERNAME=$3
PASSWD=$4
SOURCEDIR=$5
DESDIR=$( cd $6 && pwd )
cd $WORKDIR

FILENAME=$(echo $(basename $SOURCEDIR))

begin=`date +%s`

NOW=$(date "+[%Y-%m-%d %H:%M:%S]")
DATE=$(date +'%F')

echo "==============$NOW BEGIN=============="

if [ ! -d $DESDIR ]
then
	mkdir -p $DESDIR
fi

if [ x"$USERNAME" = x""  -o   x"$PASSWD" = x"" ]
then
	echo "username or passwd is null"
    exit 1
fi

# The host can ping success?
ping -c2 $HOST

if [ $? -ne 0 ]
then
	echo "can't connect to $HOST host"
	exit 1
else
	echo "ping $HOST success"
fi

# The host can ssh login?
/usr/bin/expect rsync-verify.exp $HOST $PORT $USERNAME $PASSWD $SOURCEDIR
VERIFY_STATUS=$?

case $VERIFY_STATUS in
	0) echo "the file $HOST:$SOURCEFILE exists and can be readed";;
	10) echo "$NOW authentication failed, please check host=$HOST with the username=$USERNAME and passwd=$PASSWD"; exit 1;;
	20) echo "$NOW can't connect to host=$HOST by ssh"; exit 1;;
	30) echo "the file $HOST:$SOURCEDIR is not exist or can't read"; exit 1;;
esac

# run rsync command
begin_rsync=`date +%s`

/usr/bin/expect ssh-rsync_pull.exp $HOST $PORT $USERNAME $PASSWD $SOURCEDIR $DESDIR
RSYNC_STATUS=$?

case $RSYNC_STATUS in
	0) echo "successed: From $HOST $SOURCEDIR TO $DESDIR";;
	60) echo "ssh timeout"; exit 1;;
	*) echo "rsync error"; exit 1;;
esac


end=`date +%s`
rsync_cost_time=`expr $end - $begin_rsync`
total_cost_time=`expr $end - $begin`
echo "rsync time cost: $rsync_cost_time"
echo "total time cost: $total_cost_time"

echo "==============$NOW END=============="

# delete rsync tmp file
if [ -e $DESDIR/.$FILENAME.* ]
then
    find $DESDIR  -name "\.$FILENAME\.*" |xargs rm
fi

if [ $RSYNC_STATUS -ne 0 ]
then
	echo "download failed" #> /dev/null  2>&1
	exit 1
else
	echo "download successed" #> /dev/null 2>&1
	exit 0		
fi

