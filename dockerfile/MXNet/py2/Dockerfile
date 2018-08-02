FROM nvcr.io/nvidia/mxnet:18.07-py2
#FROM ubuntu:16.04
# maintainer details
MAINTAINER deepbrainchain "mxnet"


#RUN \
#  echo 'DPkg::Post-Invoke {"/bin/rm -f /var/cache/apt/archives/*.deb || true";};' | tee /etc/apt/apt.conf.d/no-cache && \
#  echo "deb http://mirror.math.princeton.edu/pub/ubuntu xenial main universe" >> /etc/apt/sources.list && \
#  apt-get update -q -y && \
#  apt-get dist-upgrade -y && \
#  apt-get clean && \
#  rm -rf /var/cache/apt/* && \

# Install Oracle Java 8
#  DEBIAN_FRONTEND=noninteractive apt-get install -y wget unzip  software-properties-common && \
#  add-apt-repository -y ppa:webupd8team/java && \
#  apt-get update -q && \
#  echo debconf shared/accepted-oracle-license-v1-1 select true | debconf-set-selections && \
#  echo debconf shared/accepted-oracle-license-v1-1 seen true | debconf-set-selections && \
#  DEBIAN_FRONTEND=noninteractive apt-get install -y oracle-java8-installer && \
#  apt-get clean


RUN \
pip install --upgrade pip

RUN \
#apt-get install python3-tk && \
#pip install matplotlib  && \
#pip install requests  && \
pip install numpy  && \
#pip install scipy  && \
pip install nltk  && \
#pip install scrapy  && \
#pip install keras  && \
#pip install sequential  && \
#pip install face_recognition  && \
#pip install Image   && \
#pip install matplotlib  && \
#pip install h5py 
pip install gensim

CMD \
  ["/bin/bash"]

