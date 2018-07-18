#!/bin/bash
echo "start caffe2 mnist traing demo"

export LD_LIBRARY_PATH=/opt/caffe2/lib:/opt/caffe2/caffe2/python:/usr/local/nvidia/lib:/usr/local/nvidia/lib64

python ./mnist_training.py | tee /training_result_file