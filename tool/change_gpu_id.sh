#!/bin/bash

task_id=$1
gpu_id_old=$2
gpu_id_new=$3
container_id=$4

cpu_shares_old=$5
cpu_shares_new=$6


cpu_quota_old=$7
cpu_quota_new=$8

cpu_memory_old=$9
cpu_memory_new=$10

cpu_memory_swap_old=$11
cpu_memory_swap_new=$12

echo ${task_id}
echo ${container_id}

sudo docker stop ${task_id}
ps  -ef|grep dockerd | awk '{print $2}' | sudo xargs kill -9
sudo sed -i "s/NVIDIA_VISIBLE_DEVICES=${gpu_id_old}/NVIDIA_VISIBLE_DEVICES=${gpu_id_new}/g" /data/docker_data/containers/${container_id}/config.v2.json
sudo sed -i "s/CpuShares:${cpu_shares_old}/CpuShares:${cpu_shares_new}/g" /data/docker_data/containers/${container_id}/hostconfig.json
sudo sed -i "s/CpuQuota:${cpu_quota_old}/CpuQuota:${cpu_quota_new}/g" /data/docker_data/containers/${container_id}/hostconfig.json
sudo sed -i "s/Memory:${cpu_memory_old}/Memory:${cpu_memory_new}/g" /data/docker_data/containers/${container_id}/hostconfig.json
sudo sed -i "s/MemorySwap:${cpu_memory_swap_old}/MemorySwap:${cpu_memory_swap_new}/g" /data/docker_data/containers/${container_id}/hostconfig.json
exit


