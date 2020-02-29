#!/bin/bash


sync  #写入硬盘，防止数据丢失

sleep 30

# 释放缓存区内存的方法

#1）清理pagecache（页面缓存）

 echo 1 > /proc/sys/vm/drop_caches

#2）清理dentries（目录缓存）和inodes

 echo 2 > /proc/sys/vm/drop_caches

#3）清理pagecache、dentries和inodes

 echo 3 > /proc/sys/vm/drop_caches

