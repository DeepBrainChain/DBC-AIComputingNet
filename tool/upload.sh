#!/bin/bash

if [ $# -ne 1 ]; then
        echo "params count is 1, please check params error"
        exit
fi

DIR=$1

DIR_HASH=`ipfs add -r training_code_dir | tail -n 1 | awk '{print $2}'`


echo -n "DIR_HASH:"
echo $DIR_HASH

curl http://122.112.243.44:5001/api/v0/get?arg=/ipfs/$DIR_HASH > /dev/null

