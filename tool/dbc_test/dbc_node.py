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
from dbc_message.node_stop_training_req import *
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
        interval = 5
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
            h = msg_header()
            h.read(p)
        except EOFError:
            print("Error: msg header decode failure")
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
            print("Error: msg body decode failure")
            return

        print("body: ")
        pprint(vars(body), indent=4, width=24)

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
    node.send(make_ver_req(Host, PORT, node_id))

    msg_count=0
    i=0
    while i < 1:
        i=i+1
        #node.send(make_start_training_req("D:\\test_case\\task\\task-h2o.conf"))
        node.send(make_stop_training_req("Jc1HFzzN1YThSxThLf4JsJs4TKYL7PY2U"))
        # node.send(make_logs_req("2gfpp3MAB48e5nSEXrTUcLwXvobXh6oUyG2hTNLvNhF"))
        msg_count = msg_count + 1
        print("msg countis %d" %msg_count)
        # sleep(0.001)
        # j = 23809
        # while j < 23869:
        #     j = j+1
        #     node.send(make_get_peer_nodes_resp(j))
        #     #node.send(make_broad_cast_node_info(node_id, get_random_name()))

if __name__ == "__main__":
    main()
