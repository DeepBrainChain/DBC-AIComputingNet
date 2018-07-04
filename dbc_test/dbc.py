#!/usr/bin/env python
#coding=utf-8
from bitcoin import *
from matrix.ttypes_header import *
from matrix.dbc_service import *
from thrift.protocol.TBinaryProtocol import TBinaryProtocol
from thrift.transport.TTransport import TMemoryBuffer
from socket import *
import struct
import mt
import binascii
import time
import unittest
import base58
# redefine STOP mark from 0x00 to 0x7f
def get_random_id():
    private_key = random_key()
    pub_key = privkey_to_pubkey(private_key)
    pub_key_bin = binascii.a2b_hex(pub_key)
    pub_160 = bin_hash160(pub_key)
    random_id = base58.b58encode_check(pub_160)
    return random_id
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
    nonce = get_random_id()
    head = msg_header(magic, msg_name,nonce)
    head.write(p)

    addr_me = network_address("35.231.146.70", 21107)
    addr_you = network_address("35.231.146.70", 21107)
    core_version = 0x00020200
    pro_version = 0x00000001
    time_stamp = 201801111223
    start_height = 1
    private_key = random_key()
    pub_key = privkey_to_pubkey(private_key)
    pub_key_bin = binascii.a2b_hex(pub_key)
    pub_160 = bin_hash160(pub_key)

    node_prefix = bytes("node.0.")
    node_gen_src = node_prefix + pub_160
    node_id = base58.b58encode_check(node_gen_src)
    nodeid = node_id
    print nodeid
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
    nonce = get_random_id()
    head = msg_header(magic, msg_name,nonce)
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
    nonce = get_random_id()
    msg_name = "get_peer_nodes_req"
    head = msg_header(magic, msg_name,nonce)
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
    nonce = get_random_id()
    print nonce
    session_id = get_random_id()
    # head = msg_header(magic, msg_name)i
    head = msg_header(magic, msg_name,nonce,session_id)
    head.write(p)
    # task_list = [task]
    #task_list = [u'95T8yCfitbAFLQiUvVX8rMdvAb5mn6jhXgFoFDbomXXF7VHdr']
    c = list_training_req_body(task)
    # req = list_training_req(c)
    c.write(p)

    p.writeMessageEnd()
    m.flush()
    return pack(m)
def logs_req1(taskid):
    time.sleep(2)
    m = TMemoryBuffer()
    p = TBinaryProtocol(m)
    # magic = None, msg_name = None, nonce = None, session_id = None, exten_info = None
    magic = -506355561
    msg_name = "logs_req"
    nonce = get_random_id()
    session_id = get_random_id()
    # head = msg_header(magic, msg_name)i
    head = msg_header(magic, msg_name,nonce,session_id)
    head.write(p)
    c = logs_req_body(head_or_tail=1,number_of_lines=0,peer_nodes_list=[],task_id=taskid)
    c.write(p)

    p.writeMessageEnd()
    m.flush()
    return pack(m)
def stop_training_req1(taskid):
    time.sleep(2)
    m = TMemoryBuffer()
    p = TBinaryProtocol(m)
    # magic = None, msg_name = None, nonce = None, session_id = None, exten_info = None
    magic = -506355561
    msg_name = "stop_training_req"
    nonce = get_random_id()
    session_id = get_random_id()
    # head = msg_header(magic, msg_name)i
    head = msg_header(magic, msg_name,nonce,session_id)
    head.write(p)
    c = stop_training_req_body(task_id=taskid)
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
    nonce = get_random_id()
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
class f (unittest.TestCase):
    def test_handshake(self):
        task_id = get_random_id()
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

        packet_header_len, protocol_type, h, body = mt.decode2(rd, len(rd))
        self.assertTrue(packet_header_len > 0)
        self.assertTrue(protocol_type == 0)
        self.assertTrue(vars(h).get("msg_name") == "ver_resp")
        tcpclisock.close()
    def test_shake_hand_resp(self):
        task_id = get_random_id()
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
        packet_header_len, protocol_type, h, body = mt.decode2(rd, len(rd))
        tcpclisock.send(shake_hand_req1())
        rd = tcpclisock.recv(BUFSIZE)
        if not rd:
            print("error")
        packet_header_len, protocol_type, h, body = mt.decode2(rd, len(rd))
        self.assertTrue(packet_header_len > 0)
        self.assertTrue(protocol_type == 0)
        self.assertTrue(vars(h).get("msg_name") == "shake_hand_resp")
        tcpclisock.close()
    def test_get_peer_node_req(self):
        task_id = get_random_id()
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
        tcpclisock.send(get_peer_node_req1())
        rd = tcpclisock.recv(BUFSIZE)
        if not rd:
            print("error")
        packet_header_len, protocol_type, h, body = mt.decode2(rd, len(rd))
        self.assertTrue(packet_header_len > 0)
        self.assertTrue(protocol_type == 0)
        self.assertTrue(vars(h).get("msg_name") == "get_peer_nodes_resp")
        self.assertTrue(len(vars(body).get("peer_nodes_list")) > 0)
        self.assertTrue(vars(body).get("peer_nodes_list")[0].addr.port == 21107)
        self.assertIsNotNone(vars(body).get("peer_nodes_list")[0].peer_node_id)
        print "##--------------------------"
        tcpclisock.close()
    def test_list_training_req_validtaskid(self):
        task_id = get_random_id()
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
        task_list = ["ACEGz4NdYVfja7HN45XM3JSd53oA92Bpr"]
        tcpclisock.send(list_training_req1(task_list))
        rd = tcpclisock.recv(BUFSIZE)
        if not rd:
            print("error")
        packet_header_len, protocol_type, h, body = mt.decode2(rd, len(rd))
        self.assertTrue(packet_header_len > 0)
        self.assertTrue(protocol_type == 0)
        self.assertTrue(vars(h).get("msg_name") == "list_training_resp")
        self.assertTrue(len(vars(body).get("task_status_list")) >= 0)
        self.assertTrue(vars(body).get("task_status_list")[0].status == 16)
        self.assertTrue(vars(body).get("task_status_list")[0].task_id == "ACEGz4NdYVfja7HN45XM3JSd53oA92Bpr")
        print "##--------------------------"
        tcpclisock.close()
    def test_multi_list_training_req_validtaskid(self):
        task_id = get_random_id()
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
        task_list = ["ACEGz4NdYVfja7HN45XM3JSd53oA92Bpr",
                     "2fw3pMtTXaTT2jNDLqvtpRZioqYYh63FJ"]
        tcpclisock.send(list_training_req1(task_list))
        rd = tcpclisock.recv(BUFSIZE)
        if not rd:
            print("error")
        packet_header_len, protocol_type, h, body = mt.decode2(rd, len(rd))
        self.assertTrue(packet_header_len > 0)
        self.assertTrue(protocol_type == 0)
        self.assertTrue(vars(h).get("msg_name") == "list_training_resp")
        self.assertTrue(len(vars(body).get("task_status_list")) >= 0)
        self.assertTrue(vars(body).get("task_status_list")[0].status == 16)
        self.assertTrue(vars(body).get("task_status_list")[0].task_id == "ACEGz4NdYVfja7HN45XM3JSd53oA92Bpr")
        self.assertTrue(vars(body).get("task_status_list")[1].status == 8)
        self.assertTrue(vars(body).get("task_status_list")[1].task_id == "2fw3pMtTXaTT2jNDLqvtpRZioqYYh63FJ")
        print "##--------------------------"
        tcpclisock.close()
    def test_multi_list_training_req_oneinvalidtaskid(self):
        task_id = get_random_id()
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
        task_list = ["ACEGz4NdYVfja7HN45XM3JSd53oA92Bpr",
                     "GjxJZbA2MAu53Yyych7S5u3ecqwgTXN11"]
        tcpclisock.send(list_training_req1(task_list))
        rd = tcpclisock.recv(BUFSIZE)
        if not rd:
            print("error")
        packet_header_len, protocol_type, h, body = mt.decode2(rd, len(rd))
        self.assertTrue(packet_header_len > 0)
        self.assertTrue(protocol_type == 0)
        self.assertTrue(vars(h).get("msg_name") == "list_training_resp")
        self.assertTrue(len(vars(body).get("task_status_list")) >= 0)
        self.assertTrue(vars(body).get("task_status_list")[0].status == 16)
        self.assertTrue(vars(body).get("task_status_list")[0].task_id == "ACEGz4NdYVfja7HN45XM3JSd53oA92Bpr")
        print "##--------------------------"
        tcpclisock.close()
    def test_multi_list_training_req_twoinvalidtaskid(self):
        task_id = get_random_id()
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
        task_list = ["5dya4jpeufWAurd6AWoBXVGWuarUK4twU",
                     "GjxJZbA2MAu53Yyych7S5u3ecqwgTXN11"]
        tcpclisock.send(list_training_req1(task_list))
        rd = tcpclisock.recv(BUFSIZE)
        if not rd:
            print("error")
        self.assertTrue(len(rd) == 0)
        tcpclisock.close()
    def test_list_training_req_invalidtaskid(self):
        task_id = get_random_id()
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
        task_list = ["H2dVKSDMPYaFjChndNYEuESUpaSRUZHcD"]
        tcpclisock.send(list_training_req1(task_list))
        rd = tcpclisock.recv(BUFSIZE)
        if not rd:
            print("error")
        self.assertTrue(len(rd) == 0)
        tcpclisock.close()
    def test_logs_req_invalidtaskid(self):
        task_id = get_random_id()
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
        tcpclisock.send(logs_req1(u'H2dVKSDMPYaFjChndNYEuESUpaSRUZHcD'))
        rd = tcpclisock.recv(BUFSIZE)
        if not rd:
            print("error")
        self.assertTrue(len(rd) == 0)
        tcpclisock.close()
    def test_logs_req_validtaskid(self):
        task_id = get_random_id()
        print task_id
        Host = '39.105.47.155'
        PORT = 21107
        BUFSIZE = 10240
        ADDR = (Host, PORT)
        tcpclisock = socket(AF_INET, SOCK_STREAM)
        tcpclisock.connect(ADDR)
        tcpclisock.send(ver_req())
        rd = tcpclisock.recv(BUFSIZE)
        tcpclisock.send(logs_req1(u'DXXL9Nv5ChZcBnqh3HAAvU4NK772AqFppc3nHHWCDy2A5zZc5'))
        rd = tcpclisock.recv(BUFSIZE)
        if not rd:
            print("error")

        self.assertTrue(len(rd) > 0)
        tcpclisock.close()
    def test_start_training_req(self):
        task_id = get_random_id()
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
        tcpclisock.send(start_training_req1(task_id))
        rd = tcpclisock.recv(BUFSIZE)
        if not rd:
            print("error")
        tcpclisock.close()
        print "reconnect"
        tcpclisock = socket(AF_INET, SOCK_STREAM)
        tcpclisock.connect(ADDR)
        tcpclisock.send(ver_req())
        rd = tcpclisock.recv(BUFSIZE)
        if not rd:
            print("error")
        task_list = [task_id]
        tcpclisock.send(list_training_req1(task_list))
        print task_id
        rd = tcpclisock.recv(BUFSIZE)
        if not rd:
            print("error")
        packet_header_len, protocol_type, h, body = mt.decode2(rd, len(rd))
        self.assertTrue(packet_header_len > 0)
        self.assertTrue(protocol_type == 0)
        self.assertTrue(vars(h).get("msg_name") == "list_training_resp")
        self.assertTrue(len(vars(body).get("task_status_list")) >= 0)
        self.assertTrue(vars(body).get("task_status_list")[0].status <= 4)
        self.assertTrue(vars(body).get("task_status_list")[0].task_id == task_id)
        tcpclisock.close()
        ##get task update status
        print "get task update status"
        time.sleep(300)
        print "reconnect"
        tcpclisock = socket(AF_INET, SOCK_STREAM)
        tcpclisock.connect(ADDR)
        tcpclisock.send(ver_req())
        rd = tcpclisock.recv(BUFSIZE)
        if not rd:
            print("error")
        task_list = [task_id]
        tcpclisock.send(list_training_req1(task_list))
        rd = tcpclisock.recv(BUFSIZE)
        if not rd:
            print("error")
        packet_header_len, protocol_type, h, body = mt.decode2(rd, len(rd))
        self.assertTrue(packet_header_len > 0)
        self.assertTrue(protocol_type == 0)
        self.assertTrue(vars(h).get("msg_name") == "list_training_resp")
        self.assertTrue(len(vars(body).get("task_status_list")) >= 0)
        self.assertTrue(vars(body).get("task_status_list")[0].status == 16)
        self.assertTrue(vars(body).get("task_status_list")[0].task_id == task_id)
        tcpclisock.close()
    def test_start_training_stop_req(self):
        task_id = get_random_id()
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
        tcpclisock.send(start_training_req1(task_id))
        rd = tcpclisock.recv(BUFSIZE)
        if not rd:
            print("error")
        tcpclisock.close()
        print "reconnect"
        tcpclisock = socket(AF_INET, SOCK_STREAM)
        tcpclisock.connect(ADDR)
        tcpclisock.send(ver_req())
        rd = tcpclisock.recv(BUFSIZE)
        if not rd:
            print("error")
        task_list = [task_id]
        tcpclisock.send(list_training_req1(task_list))
        print task_id
        rd = tcpclisock.recv(BUFSIZE)
        if not rd:
            print("error")
        packet_header_len, protocol_type, h, body = mt.decode2(rd, len(rd))
        self.assertTrue(packet_header_len > 0)
        self.assertTrue(protocol_type == 0)
        self.assertTrue(vars(h).get("msg_name") == "list_training_resp")
        self.assertTrue(len(vars(body).get("task_status_list")) >= 0)
        self.assertTrue(vars(body).get("task_status_list")[0].status <= 4)
        self.assertTrue(vars(body).get("task_status_list")[0].task_id == task_id)
        tcpclisock.send(stop_training_req1(task_id))
        rd = tcpclisock.recv(BUFSIZE)
        if not rd:
            print("error")
        tcpclisock.close()
        ##get task update status
        print "get task update status"
        time.sleep(100)
        print "reconnect"
        tcpclisock = socket(AF_INET, SOCK_STREAM)
        tcpclisock.connect(ADDR)
        tcpclisock.send(ver_req())
        rd = tcpclisock.recv(BUFSIZE)
        if not rd:
            print("error")
        task_list = [task_id]
        tcpclisock.send(list_training_req1(task_list))
        rd = tcpclisock.recv(BUFSIZE)
        if not rd:
            print("error")
        packet_header_len, protocol_type, h, body = mt.decode2(rd, len(rd))
        self.assertTrue(packet_header_len > 0)
        self.assertTrue(protocol_type == 0)
        self.assertTrue(vars(h).get("msg_name") == "list_training_resp")
        self.assertTrue(len(vars(body).get("task_status_list")) >= 0)
        self.assertTrue(vars(body).get("task_status_list")[0].status == 8)
        self.assertTrue(vars(body).get("task_status_list")[0].task_id == task_id)
        tcpclisock.close()
if __name__=="__main__":
    print "test"



