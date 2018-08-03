FROM caffe2ai/caffe2:c2v0.8.1.cuda8.cudnn7.ubuntu16.04

RUN pip install --upgrade pip

RUN pip install nltk
RUN pip install numpy
RUN pip install gensim
RUN pip install pickle