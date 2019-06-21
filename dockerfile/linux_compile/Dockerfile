# dbc build
FROM ubuntu:16.04

RUN apt-get update

RUN apt-get install --yes \
	bsdmainutils \
	build-essential \
	gcc \
	g++ \
	libicu-devel \
#	libboost-all-dev \
	openssl-devel \
	libevent-devel \
	libtool \
	git \
	make \
    pkg-config \
    sudo \
    vim \
    cmake \
    wget \
    unzip \
    tar \
    python \
    python-pip\
    sshpass


WORKDIR /home/root

# ubuntu 16.04 does not provide boost 1.66.0 binary installation.
# Download boost 1.66.0 and compile from src code.

#ADD http://sourceforge.net/projects/boost/files/boost/1.66.0/boost_1_66_0.zip /home/root/

ADD https://dl.bintray.com/boostorg/release/1.66.0/source/boost_1_66_0.tar.gz /home/root

RUN tar -xvzf /home/root/boost_1_66_0.tar.gz
RUN rm -f /home/root/boost_1_66_0.tar.gz
WORKDIR /home/root/boost_1_66_0
RUN ./bootstrap.sh --prefix=/usr/local --with-libraries=all
#RUN ./b2 install
RUN ./b2 toolset=gcc cxxflags="-std=c++11 -fpie" install

# workaround for gcc 5 compile issue
RUN sed -i 's/result.count < result.max_buffers/(result.count) < result.max_buffers/g' /usr/local/include/boost/asio/detail/consuming_buffers.hpp


# Download dbc src code
WORKDIR /home/root
#RUN git clone -b dev --single-branch https://jianmink@github.com/DeepBrainChain/deepbrainchain.git
#ADD ../../deepbrainchain /home/root/

CMD ["/bin/bash"]