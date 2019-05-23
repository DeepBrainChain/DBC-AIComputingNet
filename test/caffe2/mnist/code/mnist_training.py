import numpy as np
import cPickle
import csv
# from matplotlib import pyplot as plt
# %matplotlib inline

from caffe2.python import core,workspace,model_helper,brew,optimizer
from caffe2.proto import caffe2_pb2

def AddLeNetModel(model):
    with core.DeviceScope(device_option):
        conv1 = brew.conv(model,'data', 'conv1', 1, 20, 5)
        pool1 = brew.max_pool(model, conv1, 'pool1', kernel=2, stride=2)
        conv2 = brew.conv(model, pool1, 'conv2', 20, 50, 5)
        pool2 = brew.max_pool(model, conv2, 'pool2', kernel=2, stride=2)
        fc3 = brew.fc(model, pool2, 'fc3', 50 * 4 * 4, 500)
        fc3 = brew.relu(model, fc3, fc3)
        pred = brew.fc(model, fc3, 'pred', 500, 10)
        softmax = brew.softmax(model, pred, 'softmax')
    return softmax


def AddAccuracy(model, softmax):
    accuracy = brew.accuracy(model, [softmax, 'label'], "accuracy")
    return accuracy


def AddTrainingOperators(model, softmax):
    # Loss Calculation
    xent = model.LabelCrossEntropy([softmax, 'label'])
    loss = model.AveragedLoss(xent, "loss")
    # Calculating Accuracy
    AddAccuracy(model, softmax)
    # Add loss to gradient for backpropogation
    model.AddGradientOperators([loss])
    # Initializing the SGD the solver
    opt = optimizer.build_sgd(model, base_learning_rate=0.1, policy="step", stepsize=1, gamma=0.999)


Snapshot_location='snapshots/'
def save_snapshot(model,iter_no):
    d={}
    for blob in model.GetParams():
        d[blob]=workspace.FetchBlob(blob)
    cPickle.dump(d,open(Snapshot_location+str(iter_no),'w'))



def check_val():
    accuracy = []
    start=0
    while start<val.shape[0]:
        l = val[start:start+Batch_Size,0].astype(np.int32)
        batch = val[start:start+Batch_Size,1:].reshape(l.shape[0],28,28)
        batch = batch[:,np.newaxis,...].astype(np.float32)
        batch = batch*float(1./256)
        workspace.FeedBlob("data", batch, device_option)
        workspace.FeedBlob("label", l, device_option)
        workspace.RunNet(val_model.net, num_iter=1)
        accuracy.append(workspace.FetchBlob('accuracy'))
        start+=l.shape[0]
    return np.mean(accuracy)



if __name__=="__main__":
    raw_train = np.loadtxt('../data/train.csv',delimiter=',')

    train,val = np.split(raw_train,[int(0.8*raw_train.shape[0])])
    print train.shape,val.shape

    device_option = caffe2_pb2.DeviceOption(device_type=caffe2_pb2.CUDA)


    Batch_Size = 64
    workspace.ResetWorkspace()

    training_model = model_helper.ModelHelper(name="training_net")

    gpu_no=0


    training_model.net.RunAllOnGPU(gpu_id=gpu_no, use_cudnn=True)
    training_model.param_init_net.RunAllOnGPU(gpu_id=gpu_no, use_cudnn=True)

    soft=AddLeNetModel(training_model)
    AddTrainingOperators(training_model, soft)

    workspace.RunNetOnce(training_model.param_init_net)
    workspace.CreateNet(training_model.net,overwrite=True,input_blobs=['data','label'])


    val_model = model_helper.ModelHelper(name="validation_net", init_params=False)
    val_model.net.RunAllOnGPU(gpu_id=gpu_no, use_cudnn=True)
    val_model.param_init_net.RunAllOnGPU(gpu_id=gpu_no, use_cudnn=True)
    val_soft=AddLeNetModel(val_model)
    AddAccuracy(val_model,val_soft)
    workspace.RunNetOnce(val_model.param_init_net)
    workspace.CreateNet(val_model.net,overwrite=True,input_blobs=['data','label'])



    total_iterations = 501
    Snapshot_interval=10
    total_iterations = total_iterations * 64

    print workspace.Blobs()


    accuracy = []
    val_accuracy = []
    loss = []
    lr = []
    start=0
    while start<total_iterations:
        l = train[start:start+Batch_Size,0].astype(np.int32) # labels for a given batch
        d=train[start:start+Batch_Size,1:].reshape(l.shape[0],28,28) # pixel values for each sample in the batch
        d=d[:,np.newaxis,...].astype(np.float32)
        d=d*float(1./256) # Scaling the pixel values for faster computation
        workspace.FeedBlob("data", d, device_option)
        workspace.FeedBlob("label", l, device_option)
        workspace.RunNet(training_model.net, num_iter=1)
        accuracy.append(workspace.FetchBlob('accuracy'))
        loss.append(workspace.FetchBlob('loss'))
        lr.append(workspace.FetchBlob('SgdOptimizer_0_lr_gpu0'))
        #    lr.append(workspace.FetchBlob('conv1_b_lr'))
        if start%Snapshot_interval == 0:
            save_snapshot(training_model,start)
        val_accuracy.append(check_val())
        start+=Batch_Size