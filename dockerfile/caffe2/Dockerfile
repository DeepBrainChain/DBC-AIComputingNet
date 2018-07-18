FROM nvcr.io/nvidia/caffe2:18.01-py2

#RUN echo 'DPkg::Post-Invoke {"/bin/rm -f /var/cache/apt/archives/*.deb || true";};' | tee /etc/apt/apt.conf.d/no-cache && \
#    echo "deb http://mirror.math.princeton.edu/pub/ubuntu xenial main universe" >> /etc/apt/sources.list && \
#    apt-get update -q -y && \
#    apt-get dist-upgrade -y && \
#    apt-get clean && \
#    rm -rf /var/cache/apt/*

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