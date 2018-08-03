FROM nvcr.io/nvidia/caffe2:18.01-py3

RUN pip install --upgrade pip

RUN pip install nltk
RUN pip install numpy
RUN pip install gensim
