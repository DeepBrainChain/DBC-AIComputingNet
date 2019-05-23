#!/bin/bash
#set -x
function update(){
if [ ! -d "update/" ];then
mkdir update
else
cd update&&rm -rf *
fi

wget http://114.115.219.202:8888/dbc
echo "比较dbc版本md5"
nowdbc=`md5sum dbc ../dbc|cut -d " " -f 1|sed -n '1p'`
download=`md5sum dbc ../dbc|cut -d " " -f 1|sed -n '2p'`
echo $nowdbc
echo $download
if [ "$nowdbc" != "$download" ];then
cd ..
chmod +x stopapp&&./stopapp
cp ./update/dbc .
chmod +x dbc
chmod +x startapp&&./startapp 
else
echo "版本最近无更新"
fi
}
while [ 1 = 1 ]
do
update
sleep 60

done








