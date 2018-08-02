import os
import socket
from time import sleep
from Queue import Queue
from Queue import LifoQueue
from plistlib import *
from select import select
from threading import Thread
#from cmpp import *
from threading import Thread
#from account import *
import select
from matrix import *
from matrix.net_message import net_message
import struct
from dbc_message import node_ver_resp
from matrix.ttypes import *
from matrix.ttypes_header import *
from matrix.dbc_service import *
from thrift.protocol.TBinaryProtocol import TBinaryProtocol
from thrift.transport.TTransport import TMemoryBuffer
from mt import *

BUFSIZ=10240
class TcpServerRS(Thread):
    def __init__ (self, In_sock):
        Thread.__init__(self)
        self.m_s = In_sock
        self.m_recvpool = Queue(1000)
        self.m_sendpool = Queue(1000)
    def run (self):
        while True:
            infds, outfds, errfds = select.select([self.m_s,],[self.m_s,],[],5)
            if len(infds):
                self.RecvData()
            if len(outfds):
                self.SendData()
            self.DealRecvData()

    def PutInData (self,InData):
        if self.m_sendpool.full():
            return 0
        self.m_sendpool.put(InData)

    def SendData (self):
        if self.m_sendpool.empty():
            return 0
        data = self.m_sendpool.get()
        self.m_s.send(data)

    def RecvData (self):
        if self.m_recvpool.full():
            return 0
        data = self.m_s.recv(BUFSIZ)
        self.m_recvpool.put(data)
    def GetData (self):
        if self.m_recvpool.empty():
            return 0
        return(self.m_recvpool.get())

    def DealRecvData (self):
        if self.m_recvpool.empty():
            return 0
        recvdata = self.m_recvpool.get()

        if len(recvdata) < 100:
            return
        ################################
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

        if msg_name == "ver_req":
            buff_msg = node_ver_resp.make_ver_resp()
            self.PutInData(buff_msg)
        ################################


        # cmppdata = headpdu()
        # print('len of recv data is %d' %(len(data)))
        # cmppdata.decodehead(data)
        # if cmppdata.Command_Id == CMPP_CONNECT:
        #     cmppconnectrsp = cmppconnectRsp()
        #     bindrspmsgbuf = cmppconnectrsp.encode()
        #     self.PutInData(bindrspmsgbuf)


class TcpServerAccept (Thread):
    def __init__ (self, InIPAddr, InPort):
        Thread.__init__(self)
        self.m_host = InIPAddr
        self.m_port = InPort
        self.m_s    = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.m_sq = Queue(1000)
        self.m_tcpserverrsthread = Queue(1000)
        self.m_accpets  = 0

    def sethost (self,InIPAddr):
        self.host = InIPAddr

    def setlistenport (self,InPort):
        self.port = InPort

    def bind (self):
        try:
            self.m_s.bind((self.m_host, self.m_port))
            self.m_s.listen(5)
        except socket.error, msg:
            print("Socket error! %s" %(msg))
            print("Bind Failed")
            os._exit(os.EX_USAGE)

    def run (self):
        while True:
            infds, outfds, errfds = select.select([self.m_s ,],[],[],5)
            if len(infds)!=0:
                (self.m_accepts, clientAddr) = self.m_s.accept()
                print("connect from", clientAddr)
                self.m_sq.put(self.m_accepts)
                tcpserverrs = TcpServerRS(self.m_accepts)
                self.m_tcpserverrsthread.put(tcpserverrs)
                tcpserverrs.start()

def main ():
    i = 23809
    while i < 23869:
        i = i + 1
        dbc_server = TcpServerAccept("10.10.254.198", i)
        dbc_server.bind()
        dbc_server.start()


if __name__ == "__main__":
    main()

