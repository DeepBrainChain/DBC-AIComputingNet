#!/bin/bash
version=$1
rm -rf ../package
mkdir ../package
cp ../output/dbc ../package/
cp -R ../conf/ ../package/
cp ../tool/add_dbc_user.sh ../package/
cp ../tool/p ../package/
cp ../tool/mining_install.sh ../package/
cp ../tool/stopapp ../package/
cd ../package/
tar -cvf dbc-linux-$version.tar ./*
gzip *.tar
