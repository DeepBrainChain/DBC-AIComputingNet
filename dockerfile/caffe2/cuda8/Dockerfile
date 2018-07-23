FROM caffe2ai/caffe2:c2v0.8.1.cuda8.cudnn7.ubuntu16.04


RUN apt-get update

RUN apt-get install --yes \
    curl

#install ipfs
ADD go-ipfs_v0.4.15_linux-amd64.tar.gz /opt
RUN  cd /opt/go-ipfs && \
     ./install.sh  && \
     rm -rf /opt/go-ipfs

RUN pip install ipfsapi

ADD swarm.key /
ADD dbc_task.sh /
ADD upload_training_result.py /

RUN cd / && \
    chmod +x dbc_task.sh