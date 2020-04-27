#!/bin/bash

task_id=$1
gpu_id_old=$2
gpu_id_new=$3
container_id=$4

echo ${task_id}
echo ${container_id}

sudo docker stop ${task_id}
ps -ef|grep dockerd | awk '{print $2}' |sudo xargs kill -9

sudo sed -i "s/NVIDIA_VISIBLE_DEVICES=${gpu_id_old}/NVIDIA_VISIBLE_DEVICES=${gpu_id_new}/g" /data/docker_data/containers/${container_id}/config.v2.json

sleep 10s
sudo docker start ${task_id}