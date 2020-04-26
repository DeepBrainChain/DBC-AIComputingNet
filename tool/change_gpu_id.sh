#!/bin/bash

task_id=$1
gpu_id_old=$2
gpu_id_new=$3
container_id=$4
echo ${task_id}
echo ${container_id}

docker stop ${task_id}

users=$(ss -tnlp |grep dockerd | awk '{print $6}')
pid_fd=$(echo ${users#*=})
pid=$(echo ${pid_fd%,*})
echo ${pid}
kill -9 ${pid}

sed -i "s/NVIDIA_VISIBLE_DEVICES=${gpu_id_old}/NVIDIA_VISIBLE_DEVICES=${gpu_id_new}/g" /data/docker_data/containers/${container_id}/config.v2.json

sleep 10s
docker start ${task_id}