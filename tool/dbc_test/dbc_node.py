from threading import Thread
from Queue import Queue
from tcpserver import *
from tcpclient import *

import sys
import unittest
import types
import time
from pprint import pprint
from matrix.ttypes import *
from matrix.ttypes_header import *
from matrix.dbc_service import *
from thrift.protocol.TBinaryProtocol import TBinaryProtocol
from thrift.transport.TTransport import TMemoryBuffer
from socket import *
import struct
import mt
import binascii

from dbc_message.node_ver_req import *
from dbc_message.node_start_training_req import  *
from dbc_message.node_broad_cast_node_info import *
from dbc_message.node_get_peer_rsp import *
from dbc_message.node_logs_req import *
import threading
from  tcpserver import *
from threading import Thread
from matrix.ttypes_header import *
from bitcoin import *
import base58
from mt import *

class dbc_node(Thread):
    def __init__ (self, speed, node_id, inpeer_ip, in_port):
        Thread.__init__(self)
        self.speed    = speed
        self.node_id  = node_id
        self.m_recvpool    = Queue(1000)
        self.m_peerip      = inpeer_ip
        self.m_peerport    = in_port
        self.dbc_client = TcpClient(inpeer_ip, in_port)
        self.dbc_client.connect()
        self.dbc_client.start()

    def setspeed (self,Inspeed):
        self.m_speed = Inspeed

    def make_shake_hand(self):
        m = TMemoryBuffer()
        p = TBinaryProtocol(m)
        msg_name = SHAKE_HAND_REQ
        head = msg_header(magic, msg_name)
        head.write(p)
        req = shake_hand_req()
        req.write(p)
        p.writeMessageEnd()
        m.flush()
        buff = pack_head(m)
        send_s = binascii.hexlify(buff)
        print(send_s)
        self.dbc_client.PutInData(buff)
        # buff_s=make_start_training_req("D:\\test_case\\task\\task-h2o.conf")
        # print(binascii.hexlify(buff_s))
        # self.dbc_client.PutInData(buff_s)
        #interval = random.randint(1, 20)
        interval = 5
        # print("shake hand interval %d" %interval)
        timer = threading.Timer(interval, self.make_shake_hand)
        timer.start()

    def deal_ver_resp(self):
        timer = threading.Timer(5, self.make_shake_hand)
        timer.start()

    def send(self, buff):
        self.dbc_client.PutInData(buff)
    def close(self):
        self.dbc_client.close()

    def DealRecv (self):
        if self.dbc_client.IsHaveData() == 0:
            return True
        recvdata = self.dbc_client.GetData()
        lenth = len(recvdata)
        if lenth <=0:
            return False

        m = TMemoryBuffer(recvdata)
        p = TBinaryProtocol(m)

        packet_header_len, protocol_type = decode_packet_header(p)
        try:
            # h = msg_header()
            h = msg_header()
            h.read(p)
        except EOFError:
            print "Error: msg header decode failure"
            return
        print("package_len:")
        print(packet_header_len)
        print "msg header: "
        pprint(vars(h), indent=4)
        msg_name = h.msg_name


        try:
            s = h.msg_name + "_body"
            if s in globals():
                t = globals()[s]
            else:
                t = empty

            body = t()
            body.read(p)
        except EOFError:
            print "Error: msg body decode failure"
            return

        print "body: "
        pprint(vars(body), indent=4, width=24)
        print

        if msg_name=="ver_resp":
            self.deal_ver_resp()
        return True

    def run (self):
        #self.makeconnect()
        while True:
            # if self.m_IsAlive == False:
            sleep(1)
            #     continue
            self.DealRecv()
            # self.MakeSubmit()


def main ():
    peer_addr=[
               "127.0.0.1:21107 ","114.116.19.45:21107","114.116.21.175:21107","49.51.47.187:21107","49.51.47.174:21107","35.237.254.158:21107",
                "35.227.90.8:21107","18.221.213.48:21107","18.188.157.102:21107","114.116.41.44:21107","114.115.219.202:21107",
                "39.107.81.6:21107","47.93.24.54:21107", "fe80::f816:3eff:fe44:afc9:21107","fe80::f816:3eff:fe33:d8db:21107"
    ]
    # derive_dbcprivate_key_node();
    Host = peer_addr[0].split(":")[0]
    PORT = int(peer_addr[0].split(":")[1])

    node_id = gen_node_id()
    print("node id: ", node_id)
    # node_id = "2gfpp3MAB48e5nSEXrTUcLwXvobXh6oUyG2hTNLvNhF"
    node = dbc_node(100000, node_id, Host, PORT)
    node.start()
    # node.close()
    # (peer_ip, peer_port,node_id)
    node.send(make_ver_req(Host, PORT, node_id))

    msg_count=0
    i=0
    while i < 1:
        i=i+1
        node.send(make_start_training_req("D:\\test_case\\task\\task-h2o.conf"))
        # node.send(make_logs_req("2gfpp3MAB48e5nSEXrTUcLwXvobXh6oUyG2hTNLvNhF"))
        msg_count = msg_count + 1
        print("msg countis %d" %msg_count)
        # sleep(0.001)
        #node.send(make_logs_req("2gfpp3MAB48e5nSEXrTUcLwXvobXh6oUyG2hTNLvNhF"))
        # pack1="000000D700000000080001E1D1A0970B0002000000116C6973745F747261696E696E675F7265710B00030000003139517331686A5458736D6178745932354D6354486738634A506E414143386A42616948367359764D636253655058484E530B000400000032324C50525241376D3936"
        # pack2="4C50525241376D3936715154334D654836543574596F396D37696F5362666E43537A453857785A35704574503752586F447F0F00010C000000010B00010000003137505435376945703431533462565153416B6D4E5661636F445A5A64684177446B79576B3477316B566D684850566E3831030002017F7F"
        # pack3="000000D700000000080001E1D1A0970B0002000000116C6973745F747261696E696E675F7265710B00030000003139517331686A5458736D6178745932354D6354486738634A506E414143386A42616948367359764D636253655058484E530B000400000032324C50525241376D3936715154334D654836543574596F396D37696F5362666E43537A453857785A35704574503752586F447F0F00010B000000010000003137505435376945703431533462565153416B6D4E5661636F445A5A64684177446B79576B3477316B566D684850566E38317F"

        # buff1=bytes().fromhex(pack1)
        # buff2=bytes().fromhex(pack2)
        # node.send(make_start_training_req("D:\\test_case\\task\\task-h2o.conf"))
        # node.send(buff1)
        # node.send(buff2)
        # node.send(buff2)
        # j = 23809
        # while j < 23869:
        #     j = j+1
        #     node.send(make_get_peer_nodes_resp(j))
        #     #node.send(make_broad_cast_node_info(node_id, get_random_name()))

if __name__ == "__main__":
    main()
