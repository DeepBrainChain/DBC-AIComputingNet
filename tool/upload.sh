#!/bin/bash

if [ $# -ne 1 ]; then
        echo "no dir path param, please input upload dir path"
        exit
fi

if [ ! -d $1 ]; then
  echo -n "dir not exist and eixt: "
  echo $1
  exit 1
fi

DIR=$1

DIR_HASH=`ipfs add -r $DIR | tail -n 1 | awk '{print $2}'`

if [ ${PIPESTATUS[0]} -ne 0 ]
then
    echo -n "ipfs add error and exit:"
	echo $1
    exit 1
fi

echo -n "DIR_HASH:"
echo $DIR_HASH

curl http://122.112.243.44:5001/api/v0/get?arg=/ipfs/$DIR_HASH > /dev/null

echo -n "DIR_HASH:"
echo $DIR_HASH