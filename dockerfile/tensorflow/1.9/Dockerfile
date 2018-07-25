FROM tensorflow/tensorflow:latest-gpu


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