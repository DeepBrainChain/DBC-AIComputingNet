#!/usr/bin/env python
#coding=utf-8
from matrix.ttypes_header import *
from matrix.dbc_service import *
from thrift.protocol.TBinaryProtocol import TBinaryProtocol
from thrift.transport.TTransport import TMemoryBuffer
from socket import *
import struct
import mt
import binascii
import time

# redefine STOP mark from 0x00 to 0x7f
def pack(tMemoryBuffer):
    code = struct.pack('!L', tMemoryBuffer.getvalue().__len__() + 8)
    code += struct.pack('!L', 0)
    values = tMemoryBuffer.getvalue()
    i = 0
    while i < values.__len__():
        code = code + struct.pack('!s', values[i])
        i = i + 1
    while i < values.__len__():
        code = code + struct.pack('!s', values[i])
        i = i + 1
    str1=binascii.hexlify(code)
    str2=""
    for i in range(0, len(str1), 2):
        n = str1[i:i + 2]
        str2 = str2+n+ " "
    str2.strip()
    print str2
    return code

def ver_req():
    m = TMemoryBuffer()
    p = TBinaryProtocol(m)
    # magic = None, msg_name = None, nonce = None, session_id = None, exten_info = None
    magic = -506355561
    msg_name = "ver_req"
    head = msg_header(magic, msg_name)
    head.write(p)

    addr_me = network_address("35.231.146.70", 21107)
    addr_you = network_address("35.231.146.70", 21107)
    core_version = 0x00020200
    pro_version = 0x00000001
    time_stamp = 201801111223
    start_height = 1
    nodeid = "111xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx111"
    req = ver_req_body(nodeid, core_version, pro_version, time_stamp, addr_me, addr_you, start_height)
    req.write(p)

    p.writeMessageEnd()
    m.flush()
    # send_s = binascii.hexlify(pack(m))
    return pack(m)
def shake_hand_req1():
    m = TMemoryBuffer()
    p = TBinaryProtocol(m)
    # magic = None, msg_name = None, nonce = None, session_id = None, exten_info = None
    magic = -506355561
    msg_name = "shake_hand_req"
    head = msg_header(magic, msg_name)
    head.write(p)

    req = shake_hand_req()
    req.write(p)

    p.writeMessageEnd()
    m.flush()
    return pack(m)
def get_peer_node_req1():
    time.sleep(2)
    m = TMemoryBuffer()
    p = TBinaryProtocol(m)
    # magic = None, msg_name = None, nonce = None, session_id = None, exten_info = None
    magic = -506355561
    msg_name = "get_peer_nodes_req"
    head = msg_header(magic, msg_name)
    head.write(p)

    req = get_peer_nodes_req()
    req.write(p)

    p.writeMessageEnd()
    m.flush()
    return pack(m)
def list_training_req1(task):
    time.sleep(2)
    m = TMemoryBuffer()
    p = TBinaryProtocol(m)
    # magic = None, msg_name = None, nonce = None, session_id = None, exten_info = None
    magic = -506355561
    msg_name = "list_training_req"
    nonce = ""+str(time.time())
    session_id = "sk5Zmt4Pm7hUXKSBBFUhFkv8bC6eGUbrBFmCvPV5y7QXNcccc999"
    # head = msg_header(magic, msg_name)i
    head = msg_header(magic, msg_name,nonce,session_id)
    head.write(p)
    task_list = [task]
    #task_list = [u'95T8yCfitbAFLQiUvVX8rMdvAb5mn6jhXgFoFDbomXXF7VHdr']
    c = list_training_req_body(task_list)
    # req = list_training_req(c)
    c.write(p)

    p.writeMessageEnd()
    m.flush()
    return pack(m)
def start_training_req1(task_id):
    time.sleep(2)
    m = TMemoryBuffer()
    p = TBinaryProtocol(m)
    # magic = None, msg_name = None, nonce = None, session_id = None, exten_info = None
    magic = -506355561
    msg_name = "start_training_req"
    nonce = "ib7hzRqwfmM11LsY2QJXN6U7mquwNwi9pepGH2drrdssU"+str(time.time())
    session_id = None
    # head = msg_header(magic, msg_name)i
    head = msg_header(magic, msg_name,nonce,session_id)
    head.write(p)
    task_id = task_id
    select_mode = 48
    master = u'2gfpp3MAB42jWqp5M9aCRNt3gKitmt7wzgJ4AnUFoTt'
    peer_nodes_list = [u'2gfpp3MAB42jWqp5M9aCRNt3gKitmt7wzgJ4AnUFoTt']
    server_specification = u'4'
    server_count = u'1'
    training_engine = 0
    code_dir = u'QmUWvH29EfttHT9TrowdesFT39Ja7u7cRC5NaM5hCJP6Rp'
    entry_file = u'hellotensorflow.py'
    data_dir = u'QmbTLM6hkHsjDCDJ4P3vw1kr51jyKHvjvzVPLGB7kTqcww'
    checkpoint_dir = u'/dbc'
    hyper_parameters = u'batch_size:32;learning_rate:0.001;learning_rate_decay=0.8;'
    c=start_training_req_body(task_id,select_mode,master,peer_nodes_list,server_specification,server_count,training_engine,code_dir,entry_file,data_dir,checkpoint_dir,hyper_parameters)
    # req = list_training_req(c)
    c.write(p)
    p.writeMessageEnd()
    m.flush()
    #send_s = binascii.hexlify(code)
    return pack(m)

if __name__=="__main__":
    print "中文"
    task_id = u'k'+str(time.time())
    print task_id
    Host = '39.105.47.155'
    PORT = 21107
    BUFSIZE = 10240
    ADDR = (Host, PORT)
    tcpclisock = socket(AF_INET, SOCK_STREAM)
    tcpclisock.connect(ADDR)
    tcpclisock.send(ver_req())

    rd = tcpclisock.recv(BUFSIZE)
    if not rd:
        print("error")

    recv_s = binascii.hexlify(rd)

    mt.decode2(rd,len(rd))
    print "##--------------------------"
    ##--------------------------
    tcpclisock.send(shake_hand_req1())

    rd = tcpclisock.recv(BUFSIZE)
    if not rd:
        print("error")

    recv_s = binascii.hexlify(rd)
    mt.decode2(rd, len(rd))
    print "##--------------------------"
    tcpclisock.send(get_peer_node_req1())

    rd = tcpclisock.recv(BUFSIZE)
    if not rd:
        print("error")

    recv_s = binascii.hexlify(rd)

    mt.decode2(rd, len(rd))
    print "##--------------------------"
    ##--------------------------
    tcpclisock.send(list_training_req1("k1530189402.26"))
    rd = tcpclisock.recv(BUFSIZE)
    if not rd:
        print("error")

    recv_s = binascii.hexlify(rd)

    mt.decode2(rd, len(rd))
    print "##--------------------------"
    ##暂停三分钟
    ##--------------------------

    tcpclisock.send(start_training_req1(task_id))

    rd = tcpclisock.recv(BUFSIZE)
    if not rd:
        print("error")

    ##重新连接
    print "重新连接"
    tcpclisock = socket(AF_INET, SOCK_STREAM)
    tcpclisock.connect(ADDR)
    tcpclisock.send(ver_req())

    rd = tcpclisock.recv(BUFSIZE)
    if not rd:
        print("error")
    ##--------------------------
    tcpclisock.send(shake_hand_req1())
    rd = tcpclisock.recv(BUFSIZE)
    if not rd:
        print("error")
    tcpclisock.send(list_training_req1(task_id))
    rd = tcpclisock.recv(BUFSIZE)
    if not rd:
        print("error")

    recv_s = binascii.hexlify(rd)

    mt.decode2(rd, len(rd))
    ##获取任务更新状态
    print "获取任务更新状态"
    time.sleep(300)
    print "重新连接"
    tcpclisock = socket(AF_INET, SOCK_STREAM)
    tcpclisock.connect(ADDR)
    tcpclisock.send(ver_req())

    rd = tcpclisock.recv(BUFSIZE)
    if not rd:
        print("error")
    ##--------------------------
    tcpclisock.send(shake_hand_req1())
    rd = tcpclisock.recv(BUFSIZE)
    if not rd:
        print("error")
    tcpclisock.send(list_training_req1(task_id))
    rd = tcpclisock.recv(BUFSIZE)
    if not rd:
        print("error")

    recv_s = binascii.hexlify(rd)

    mt.decode2(rd, len(rd))


