#!/bin/bash
echo "start caffe2 relu op test"

export LD_LIBRARY_PATH=/opt/caffe2/lib:/opt/caffe2/caffe2/python:/usr/local/nvidia/lib:/usr/local/nvidia/lib64
python -m caffe2.python.operator_test.relu_op_test
