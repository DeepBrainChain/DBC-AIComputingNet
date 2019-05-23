
label
-
dbctraining/caffe2-gpu:v0.0.1

content
-
- python 2.7
- caffe2 0.8.1
- cuda8
    - cudnn7
    - ubuntu16.04


how to build image
-
note: context_dir has dbc_task.sh, swarm.key and other files.
```
# ls -l ./context/
total 9984
-rw-r--r-- 1 dbc dbc     3749 Jun 28 08:57 dbc_task.sh
-rw-r--r-- 1 dbc dbc 10209630 Jun 26 13:04 go-ipfs_v0.4.15_linux-amd64.tar.gz
-rw-r--r-- 1 dbc dbc       95 Jun 26 13:03 swarm.key
-rw-r--r-- 1 dbc dbc      398 Jun 26 13:03 upload_training_result.py

# docker build -t dbctraining/caffe2-gpu:v0.0.1 -f ./Dockerfile.caffe2.cuda8 <context_dir>
```

