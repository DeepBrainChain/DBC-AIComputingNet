
#!/bin/bash

task_id=$1
gpu_id_old=$2
gpu_id_new=$3
container_id=$4

cpu_shares=$5
cpu_shares_array=(${cpu_shares//,/ })
cpu_shares_old=${cpu_shares_array[0]}
cpu_shares_new=${cpu_shares_array[1]}

cpu_quota=$6
cpu_quota_array=(${cpu_quota//,/ })
cpu_quota_old=${cpu_quota_array[0]}
cpu_quota_new=${cpu_quota_array[1]}

cpu_memory=$7
cpu_memory_array=(${cpu_memory//,/ })
cpu_memory_old=${cpu_memory_array[0]}
cpu_memory_new=${cpu_memory_array[1]}

cpu_memory_swap=$8
cpu_memory_swap_array=(${cpu_memory_swap//,/ })
cpu_memory_swap_old=${cpu_memory_swap_array[0]}
cpu_memory_swap_new=${cpu_memory_swap_array[1]}

docker_dir=$9
sudo docker stop ${task_id}
ps  -ef|grep dockerd | awk '{print $2}' | sudo xargs kill -9
sudo sed -i "s/NVIDIA_VISIBLE_DEVICES=.*\"/NVIDIA_VISIBLE_DEVICES=${gpu_id_new}\"/g" ${docker_dir}containers/${container_id}/config.v2.json

sudo sed -i "s/CpuShares\":.*,/CpuShares\":${cpu_shares_new},/g" ${docker_dir}containers/${container_id}/hostconfig.json
sudo sed -i "s/CpuQuota\":.*,/CpuQuota\":${cpu_quota_new},/g" ${docker_dir}containers/${container_id}/hostconfig.json
sudo sed -i "s/Memory\":.*,/Memory\":${cpu_memory_new},/g" ${docker_dir}containers/${container_id}/hostconfig.json
sudo sed -i "s/MemorySwap\":.*,/MemorySwap\":${cpu_memory_swap_new},/g" ${docker_dir}containers/${container_id}/hostconfig.json
exit